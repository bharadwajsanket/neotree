/*
 * main.c -- neotree entry point
 *
 * Responsibilities:
 *   1. Parse CLI arguments via cli_parse().
 *   2. Initialise color output.
 *   3. Validate that the root path exists and is a directory.
 *   4. Open export file streams (--export-txt / --export-markdown).
 *   5. Register export file basenames as ignored entries so they do
 *      not appear inside the generated tree (Issue 3).
 *   6. Print the root label.
 *   7. Hand off to tree_walk().
 *   8. Print the summary line.
 *   9. Print --ext-summary if requested.
 *  10. Finalise export files (close; add markdown fence).
 *  11. Release opts memory via cli_opts_free().
 *
 * Issue 2 (--dirs-only summary):
 *   When --dirs-only is active the summary line omits the file count
 *   because files are intentionally not shown.
 *
 * Issue 3 (export self-inclusion):
 *   The basename of each export target file is added to the temporary
 *   ignore list before traversal so it cannot appear in the tree.
 *   This works via the existing ignore pipeline with zero special cases
 *   in tree.c or utils.c.
 *
 * Issue 4 (path portability):
 *   fs_is_dir() uses stat() which follows symlinks correctly on POSIX.
 *   Trailing slashes in the root path are accepted (stat handles them).
 *   On macOS, system headers live under the Xcode SDK, not /usr/include;
 *   if /usr/include is absent, neotree correctly reports it as missing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "cli.h"
#include "tree.h"
#include "utils.h"
#include "fs.h"
#include "find.h"

/* ------------------------------------------------------------------ */
/*  Windows console initialisation                                      */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
#  include <windows.h>
/*
 * win32_console_init -- set UTF-8 code page and enable ANSI processing.
 *
 * Without this, box-drawing characters (├── └── │) render as mojibake
 * and ANSI color escapes print as literal text.
 *
 * SetConsoleOutputCP(CP_UTF8) / SetConsoleCP(CP_UTF8):
 *   Set the console to code page 65001 (UTF-8) so UTF-8 bytes written
 *   via printf/fputs are decoded and displayed correctly.  Safe to call
 *   unconditionally — when stdout is redirected the bytes go to the file
 *   unchanged and the code page setting has no effect.
 *
 * ENABLE_VIRTUAL_TERMINAL_PROCESSING:
 *   Enables ANSI/VT escape sequence interpretation in the Windows console
 *   host (supported since Windows 10 build 1511).  Only set when stdout
 *   is a real console; GetConsoleMode fails gracefully on a pipe/file.
 *
 * Both calls are best-effort: failure is silently ignored.  The program
 * remains functional — just with degraded rendering — if either fails.
 */
static void win32_console_init(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode))
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}
#endif /* _WIN32 */

#ifdef __GNUC__
static void print_all(FILE *out, FILE *export_txt, FILE *export_md, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
#endif

static void print_all(FILE *out, FILE *export_txt, FILE *export_md, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (out) fputs(buf, out);
    if (export_txt) fputs(buf, export_txt);
    if (export_md)  fputs(buf, export_md);
}

static int main_ext_sort_cmp(const void *a, const void *b) {
    const ext_entry_t *ea = (const ext_entry_t *)a;
    const ext_entry_t *eb = (const ext_entry_t *)b;
    if (ea->count != eb->count)
        return eb->count - ea->count;
    return strcmp(ea->ext, eb->ext);
}



int main(int argc, char *argv[]) {
#ifdef _WIN32
    win32_console_init();
#endif

    /* ---- parse args ---- */
    cli_opts_t opts;
    cli_parse(argc, argv, &opts);

    /* ---- color init (respects --no-color + isatty) ---- */
    color_init(opts.no_color);

    /* ---- isolated find mode ---- */
    if (opts.find || opts.find_dir) {
        int exit_code = 0;
        for (int r = 0; r < opts.roots_count; r++) {
            const char *root = opts.roots[r];
            if (!fs_is_dir(root)) {
                fprintf(stderr, "neotree: '%s' is not a directory or does not exist\n", root);
                exit_code = 1;
                continue;
            }
            find_walk(root, "", &opts, 0);
        }
        cli_opts_free(&opts);
        return exit_code;
    }

    /* ---- open export streams ---- */
    FILE *export_txt = NULL;
    FILE *export_md  = NULL;

    if (opts.export_txt) {
        export_txt = fopen(opts.export_txt, "w");
        if (!export_txt) {
            fprintf(stderr, "neotree: cannot open '%s' for writing\n",
                    opts.export_txt);
            cli_opts_free(&opts);
            return 1;
        }
    }

    if (opts.export_md) {
        export_md = fopen(opts.export_md, "w");
        if (!export_md) {
            fprintf(stderr, "neotree: cannot open '%s' for writing\n",
                    opts.export_md);
            if (export_txt) fclose(export_txt);
            cli_opts_free(&opts);
            return 1;
        }
        /* Open fenced code block */
        fprintf(export_md, "```\n");
    }

    int exit_code = 0;
    int printed_roots = 0;

    for (int r = 0; r < opts.roots_count; r++) {
        const char *root = opts.roots[r];

        if (!fs_is_dir(root)) {
            fprintf(stderr, "neotree: '%s' is not a directory or does not exist\n", root);
            exit_code = 1;
            continue;
        }

        if (printed_roots > 0) {
            print_all(stdout, export_txt, export_md, "\n");
        }
        printed_roots++;

        /* Merge .gitignore patterns from root directory */
        cli_load_gitignore(&opts, root);

        /* ---- ext-summary table ---- */
        ext_table_t ext_tbl;
        ext_table_t *ext_tbl_ptr = NULL;
        if (opts.show_stats && !opts.dirs_only) {
            ext_table_init(&ext_tbl);
            ext_tbl_ptr = &ext_tbl;
        }

        /* ---- print root label ---- */
        const char *rc = color_for_name(root, 1);
        const char *rs = color_reset();
        size_t root_len = strlen(root);
        int needs_slash = (root_len > 0 &&
                           root[root_len - 1] != '/' &&
                           root[root_len - 1] != '\\');
        printf("%s%s%s%s\n", rc, root, needs_slash ? "/" : "", rs);
        if (export_txt) fprintf(export_txt, "%s%s\n", root, needs_slash ? "/" : "");
        if (export_md)  fprintf(export_md,  "%s%s\n", root, needs_slash ? "/" : "");

        /* ---- primary walk (stdout + optional txt export) ---- */
        tree_stats_t stats = { 0, 0, 0, "", -1, 0 };
        tree_walk(root, "", "", &opts, 0, &stats,
                  stdout, export_txt, ext_tbl_ptr);

        /* ---- markdown export pass ---- */
        if (export_md) {
            tree_stats_t stats2 = { 0, 0, 0, "", -1, 0 };
            tree_walk(root, "", "", &opts, 0, &stats2,
                      export_md, NULL, NULL);
        }

        /* ---- summary line ---- */
        char summary[160];
        int n;

        if (opts.dirs_only) {
            n = snprintf(summary, sizeof(summary),
                         "\n%d director%s\n",
                         stats.total_dirs,
                         stats.total_dirs == 1 ? "y" : "ies");
        } else {
            n = snprintf(summary, sizeof(summary),
                         "\n%d director%s, %d file%s\n",
                         stats.total_dirs,  stats.total_dirs  == 1 ? "y" : "ies",
                         stats.total_files, stats.total_files == 1 ? "" : "s");
        }
        (void)n;

        fputs(summary, stdout);
        if (export_txt) fputs(summary, export_txt);
        if (export_md)  fputs(summary, export_md);

        /* ---- stats summary ---- */
        if (opts.show_stats) {
            print_all(stdout, export_txt, export_md, "\nStats:\n");
            if (opts.dirs_only) {
                print_all(stdout, export_txt, export_md, "  %-17s%d\n", "directories", stats.total_dirs);
                print_all(stdout, export_txt, export_md, "  %-17s%d\n", "max depth", stats.max_depth);
            } else {
                char size_buf[64];
                format_size(stats.total_size_bytes, size_buf, sizeof(size_buf));

                print_all(stdout, export_txt, export_md, "  %-17s%d\n", "directories", stats.total_dirs);
                print_all(stdout, export_txt, export_md, "  %-17s%d\n", "files", stats.total_files);
                print_all(stdout, export_txt, export_md, "  %-17s%s\n", "total size", size_buf);

                if (ext_tbl_ptr && ext_tbl_ptr->len > 0) {
                    print_all(stdout, export_txt, export_md, "\nExtensions:\n");
                    ext_entry_t sorted[EXT_TABLE_MAX];
                    int n_ext = ext_tbl_ptr->len;
                    for (int i = 0; i < n_ext; i++) sorted[i] = ext_tbl_ptr->entries[i];
                    qsort(sorted, (size_t)n_ext, sizeof(ext_entry_t), main_ext_sort_cmp);
                    for (int i = 0; i < n_ext; i++) {
                        const char *label = sorted[i].ext[0] ? sorted[i].ext : "(no ext)";
                        print_all(stdout, export_txt, export_md, "  %-17s%d\n", label, sorted[i].count);
                    }
                }
            }
        }

        cli_gitignore_free(&opts);
    }

    /* ---- finalise exports ---- */
    if (export_md) {
        fprintf(export_md, "```\n");
        fclose(export_md);
    }
    if (export_txt) {
        fclose(export_txt);
    }

    /* ---- release heap memory owned by opts ---- */
    cli_opts_free(&opts);

    return exit_code;
}
