/*
 * main.c -- neotree entry point
 *
 * Responsibilities:
 *   1.  Parse CLI arguments via cli_parse().
 *   2.  Initialise color output and terminal-width detection.
 *   3.  Validate that the root path exists and is a directory.
 *   4.  Dispatch to an isolated mode when applicable:
 *         --find / --find-dir  → find_walk()
 *         --largest N          → largest_files_print()
 *         --largest-dirs N     → largest_dirs_print()
 *   5.  Open export file streams (--export-txt / --export-markdown /
 *       --export-json).
 *   6.  Register export file basenames as ignored entries so they do
 *       not appear inside the generated tree (Issue 3).
 *   7.  Print the root label.
 *   8.  Hand off to tree_walk() (single-pass traversal handles stdout
 *       as well as txt, md, and JSON exports).
 *   9.  Print the summary line.
 *  10.  Print --stats summary (with largest file, largest dir,
 *       deepest path).
 *  11.  Finalise export files (close; add markdown fence; JSON newline).
 *  12.  Release opts memory via cli_opts_free().
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
#include "largest.h"

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
static void print_all(FILE *out, FILE *export_txt, FILE *export_md,
                       const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
#endif

static void print_all(FILE *out, FILE *export_txt, FILE *export_md,
                       const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (out)        fputs(buf, out);
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

static void suggest_wildcard_hint(const char *root, const cli_opts_t *opts) {
    const char *opt_name = NULL;
    const char *val = NULL;
    if (opts->find)     { opt_name = "--find";     val = opts->find;    }
    else if (opts->find_dir) { opt_name = "--find-dir"; val = opts->find_dir; }
    else if (opts->pattern)  { opt_name = "--pattern";  val = opts->pattern;  }

    if (opt_name && val) {
        const char *val_ext  = strrchr(val,  '.');
        const char *root_ext = strrchr(root, '.');
        if (val_ext && root_ext && strcmp(val_ext, root_ext) == 0) {
            fprintf(stderr, "\nHint:\n");
            fprintf(stderr, "  Did you mean:\n\n");
            fprintf(stderr, "    neotree %s \"*%s\"\n\n", opt_name, val_ext);
            fprintf(stderr, "  Shell expansion expanded the wildcard "
                            "before neotree received it.\n\n");
        }
    }
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    win32_console_init();
#endif

    /* ---- parse args ---- */
    cli_opts_t opts;
    cli_parse(argc, argv, &opts);

    /* ---- color + terminal-width init ---- */
    color_init(opts.no_color);
    term_width_init();

    /* ---- isolated find mode ---- */
    if (opts.find || opts.find_dir) {
        int exit_code = 0;
        for (int r = 0; r < opts.roots_count; r++) {
            const char *root = opts.roots[r];
            if (!fs_is_dir(root)) {
                fprintf(stderr,
                        "neotree: '%s' is not a directory or does not exist\n",
                        root);
                suggest_wildcard_hint(root, &opts);
                exit_code = 1;
                continue;
            }
            find_walk(root, "", &opts, 0);
        }
        cli_opts_free(&opts);
        return exit_code;
    }

    /* ---- isolated --largest mode ---- */
    if (opts.largest_n > 0) {
        int exit_code = 0;
        for (int r = 0; r < opts.roots_count; r++) {
            const char *root = opts.roots[r];
            if (!fs_is_dir(root)) {
                fprintf(stderr,
                        "neotree: '%s' is not a directory or does not exist\n",
                        root);
                exit_code = 1;
            } else {
                cli_load_gitignore(&opts, root);
            }
        }
        if (exit_code == 0) {
            largest_files_print((const char * const *)opts.roots,
                                 opts.roots_count, &opts, opts.largest_n);
        }
        cli_opts_free(&opts);
        return exit_code;
    }

    /* ---- isolated --largest-dirs mode ---- */
    if (opts.largest_dirs_n > 0) {
        int exit_code = 0;
        for (int r = 0; r < opts.roots_count; r++) {
            const char *root = opts.roots[r];
            if (!fs_is_dir(root)) {
                fprintf(stderr,
                        "neotree: '%s' is not a directory or does not exist\n",
                        root);
                exit_code = 1;
            } else {
                cli_load_gitignore(&opts, root);
            }
        }
        if (exit_code == 0) {
            largest_dirs_print((const char * const *)opts.roots,
                                opts.roots_count, &opts, opts.largest_dirs_n);
        }
        cli_opts_free(&opts);
        return exit_code;
    }

    /* ---- open export streams ---- */
    FILE *export_txt  = NULL;
    FILE *export_md   = NULL;
    FILE *export_json = NULL;

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

    if (opts.export_json) {
        export_json = fopen(opts.export_json, "w");
        if (!export_json) {
            fprintf(stderr, "neotree: cannot open '%s' for writing\n",
                    opts.export_json);
            if (export_md)  fclose(export_md);
            if (export_txt) fclose(export_txt);
            cli_opts_free(&opts);
            return 1;
        }
    }

    int exit_code    = 0;
    int printed_roots = 0;

    /* For multi-root JSON: wrap all root objects in an array */
    if (export_json && opts.roots_count > 1) {
        fputs("[\n", export_json);
    }

    for (int r = 0; r < opts.roots_count; r++) {
        const char *root = opts.roots[r];

        if (!fs_is_dir(root)) {
            fprintf(stderr,
                    "neotree: '%s' is not a directory or does not exist\n",
                    root);
            suggest_wildcard_hint(root, &opts);
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
        ext_table_t  ext_tbl;
        ext_table_t *ext_tbl_ptr = NULL;
        if (opts.show_stats && !opts.dirs_only) {
            ext_table_init(&ext_tbl);
            ext_tbl_ptr = &ext_tbl;
        }

        /* ---- print root label ---- */
        {
            size_t      root_len = strlen(root);
            int         needs_slash = (root_len > 0 &&
                                       root[root_len - 1] != '/' &&
                                       root[root_len - 1] != '\\');
            if (!opts.show_stats && !opts.export_txt && !opts.export_md && !opts.export_json) {
                const char *rc = color_for_name(root, 1);
                const char *rs = color_reset();
                printf("%s%s%s%s\n", rc, root, needs_slash ? "/" : "", rs);
            }
            if (export_txt)
                fprintf(export_txt, "%s%s\n", root, needs_slash ? "/" : "");
            if (export_md)
                fprintf(export_md,  "%s%s\n", root, needs_slash ? "/" : "");
        }

        /* ---- primary walk (stdout + optional txt/markdown/json export) ---- */
        tree_stats_t stats;
        /* zero-initialise; largest_file_size_bytes starts at -1 */
        stats.total_files          = 0;
        stats.total_dirs           = 0;
        stats.total_size_bytes     = 0LL;
        stats.largest_file[0]      = '\0';
        stats.largest_file_size_bytes = -1LL;
        stats.largest_dir[0]       = '\0';
        stats.largest_dir_size_bytes  = -1LL;
        stats.deepest_path[0]      = '\0';
        stats.max_depth            = 0;

        if (export_json && r > 0) {
            fputs(",\n", export_json);
        }

        {
            FILE *walk_out = stdout;
            if (opts.show_stats || opts.export_txt || opts.export_md || opts.export_json) {
                walk_out = NULL;
            }
            tree_walk(root, "", "", &opts, 0, &stats,
                      walk_out, export_txt, export_md, export_json, ext_tbl_ptr);
        }

        if (export_json) {
            fputc('\n', export_json);
        }

        /* ---- summary line ---- */
        {
            char summary[160];
            int  n;

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

            if (!opts.show_stats && !opts.export_txt && !opts.export_md && !opts.export_json) {
                fputs(summary, stdout);
            }
            if (export_txt) fputs(summary, export_txt);
            if (export_md)  fputs(summary, export_md);
        }

        /* ---- stats summary ---- */
        if (opts.show_stats) {
            char size_buf[64];
            format_size(stats.total_size_bytes, size_buf, sizeof(size_buf));

            print_all(stdout, export_txt, export_md, "Statistics\n");
            print_all(stdout, export_txt, export_md, "----------\n\n");
            print_all(stdout, export_txt, export_md, "Directories : %d\n", stats.total_dirs);
            if (!opts.dirs_only) {
                print_all(stdout, export_txt, export_md, "Files       : %d\n", stats.total_files);
            }
            print_all(stdout, export_txt, export_md, "Max depth   : %d\n", stats.max_depth);
            if (!opts.dirs_only) {
                print_all(stdout, export_txt, export_md, "Total size  : %s\n", size_buf);
            }
            print_all(stdout, export_txt, export_md, "\n");

            int has_deepest = (stats.deepest_path[0] != '\0');
            int has_largest_file = (!opts.dirs_only && stats.largest_file[0] != '\0');
            int has_largest_dir = (!opts.dirs_only && stats.largest_dir[0] != '\0');

            if (has_deepest) {
                print_all(stdout, export_txt, export_md, "Deepest path: %s\n", stats.deepest_path);
            }
            if (has_largest_file) {
                char lf_sz[64];
                format_size(stats.largest_file_size_bytes, lf_sz, sizeof(lf_sz));
                print_all(stdout, export_txt, export_md, "Largest file: %s (%s)\n", stats.largest_file, lf_sz);
            }
            if (has_largest_dir) {
                char ld_sz[64];
                format_size(stats.largest_dir_size_bytes, ld_sz, sizeof(ld_sz));
                print_all(stdout, export_txt, export_md, "Largest dir : %s (%s)\n", stats.largest_dir, ld_sz);
            }

            if (has_deepest || has_largest_file || has_largest_dir) {
                print_all(stdout, export_txt, export_md, "\n");
            }

            /* Extension breakdown */
            if (!opts.dirs_only && ext_tbl_ptr && ext_tbl_ptr->len > 0) {
                print_all(stdout, export_txt, export_md, "Extensions\n");
                print_all(stdout, export_txt, export_md, "----------\n\n");
                ext_entry_t sorted[EXT_TABLE_MAX];
                int n_ext = ext_tbl_ptr->len;
                int i;
                for (i = 0; i < n_ext; i++) sorted[i] = ext_tbl_ptr->entries[i];
                qsort(sorted, (size_t)n_ext, sizeof(ext_entry_t), main_ext_sort_cmp);
                for (i = 0; i < n_ext; i++) {
                    const char *label = sorted[i].ext[0] ? sorted[i].ext : "(no ext)";
                    print_all(stdout, export_txt, export_md, "%-12s%d\n", label, sorted[i].count);
                }
            }
        }

        cli_gitignore_free(&opts);
    }

    /* Close multi-root JSON array */
    if (export_json && opts.roots_count > 1) {
        fputs("]\n", export_json);
    }

    /* ---- finalise exports ---- */
    int export_count = 0;
    if (opts.export_txt) export_count++;
    if (opts.export_md) export_count++;
    if (opts.export_json) export_count++;

    if (export_md) {
        fprintf(export_md, "```\n");
        fclose(export_md);
    }
    if (export_txt) {
        fclose(export_txt);
    }
    if (export_json) {
        fclose(export_json);
    }

    if (export_count > 0 && exit_code == 0) {
        if (export_count >= 2) {
            printf("✓ Export complete\n");
            if (opts.export_txt)  printf("  • %s\n", opts.export_txt);
            if (opts.export_md)   printf("  • %s\n", opts.export_md);
            if (opts.export_json) printf("  • %s\n", opts.export_json);
        } else {
            if (opts.export_txt) {
                printf("✓ Exported plain text tree to:\n  %s\n", opts.export_txt);
            } else if (opts.export_md) {
                printf("✓ Exported markdown tree to:\n  %s\n", opts.export_md);
            } else if (opts.export_json) {
                printf("✓ Exported JSON tree to:\n  %s\n", opts.export_json);
            }
        }
    }

    /* ---- release heap memory owned by opts ---- */
    cli_opts_free(&opts);

    return exit_code;
}
