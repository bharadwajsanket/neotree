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

#include "cli.h"
#include "tree.h"
#include "utils.h"
#include "fs.h"

/* ------------------------------------------------------------------ */
/*  basename_of -- return pointer to filename component of a path.     */
/*                                                                     */
/*  Does not allocate.  Returns a pointer into `path` itself.          */
/*  Works on both POSIX ("/a/b/c.txt" -> "c.txt") and Windows.        */
/* ------------------------------------------------------------------ */
static const char *basename_of(const char *path) {
    const char *p = path;
    const char *last = path;
    while (*p) {
        if (*p == '/' || *p == '\\') last = p + 1;
        p++;
    }
    return last;
}

int main(int argc, char *argv[]) {
    /* ---- parse args ---- */
    cli_opts_t opts;
    cli_parse(argc, argv, &opts);

    /* ---- color init (respects --no-color + isatty) ---- */
    color_init(opts.no_color);

    /* ---- validate root ---- */
    if (!fs_is_dir(opts.root)) {
        fprintf(stderr, "neotree: '%s' is not a directory or does not exist\n",
                opts.root);
        cli_opts_free(&opts);
        return 1;
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

    /*
     * ---- Issue 3: ignore export target filenames during traversal ----
     *
     * Add the basename of each export file to the ignore list so it
     * cannot appear inside the generated tree.  These are argv pointers
     * (not heap-allocated), so they must not be freed by cli_opts_free().
     * We insert them before gitignore_start so they are treated exactly
     * like user-supplied --ignore entries.
     *
     * We only do this if we have room in the ignore array.
     */
    if (opts.export_txt && opts.ignore_count < CLI_MAX_IGNORE) {
        opts.ignore[opts.ignore_count++] = basename_of(opts.export_txt);
        /* Keep gitignore_start in sync so free logic is correct */
        opts.gitignore_start = opts.ignore_count;
    }
    if (opts.export_md && opts.ignore_count < CLI_MAX_IGNORE) {
        opts.ignore[opts.ignore_count++] = basename_of(opts.export_md);
        opts.gitignore_start = opts.ignore_count;
    }

    /* ---- ext-summary table ---- */
    ext_table_t ext_tbl;
    ext_table_t *ext_tbl_ptr = NULL;
    if (opts.ext_summary) {
        ext_table_init(&ext_tbl);
        ext_tbl_ptr = &ext_tbl;
    }

    /* ---- print root label ---- */
    const char *rc = color_for_name(opts.root, 1);
    const char *rs = color_reset();
    printf("%s%s%s\n", rc, opts.root, rs);
    if (export_txt) fprintf(export_txt, "%s\n", opts.root);
    if (export_md)  fprintf(export_md,  "%s\n", opts.root);

    /* ---- primary walk (stdout + optional txt export) ---- */
    tree_stats_t stats = { 0, 0 };
    tree_walk(opts.root, "", "", &opts, 0, &stats,
              stdout, export_txt, ext_tbl_ptr);

    /*
     * ---- markdown export pass ----
     *
     * tree_walk accepts one export stream at a time.  If markdown is
     * requested, do a second pass writing to export_md as the primary
     * stream (ANSI codes are disabled because color_init reflects the
     * original TTY state; export_md is not a TTY, so color_enabled()
     * is already 0 when piped, but we pass NULL for export_out to
     * keep the logic clean).
     */
    if (export_md) {
        tree_stats_t stats2 = { 0, 0 };
        tree_walk(opts.root, "", "", &opts, 0, &stats2,
                  export_md, NULL, NULL);
    }

    /* ---- summary line ---- */
    char summary[160];
    int n;

    if (opts.dirs_only) {
        /*
         * Issue 2: suppress file count when --dirs-only is active.
         * Files are counted internally but intentionally not shown.
         */
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

    /* ---- ext summary (stdout only) ---- */
    if (opts.ext_summary && ext_tbl_ptr)
        ext_table_print(ext_tbl_ptr);

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

    return 0;
}
