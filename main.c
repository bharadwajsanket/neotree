/*
 * main.c — neotree entry point
 *
 * Responsibilities (and only these):
 *   1. Parse CLI arguments via cli_parse().
 *   2. Initialise color output.
 *   3. Validate that the root path exists and is a directory.
 *   4. Print the root label.
 *   5. Hand off to tree_walk().
 *   6. Print the summary line.
 *   7. Release opts memory via cli_opts_free().
 */

#include <stdio.h>
#include <stdlib.h>

#include "cli.h"
#include "tree.h"
#include "utils.h"
#include "fs.h"

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

    /* ---- print root label ---- */
    printf("%s%s%s\n",
           color_for_name(opts.root, 1),
           opts.root,
           color_reset());

    /* ---- walk ---- */
    tree_stats_t stats = { 0, 0 };
    tree_walk(opts.root, "", &opts, 0, &stats);

    /* ---- summary ---- */
    printf("\n%d director%s, %d file%s\n",
           stats.total_dirs,  stats.total_dirs  == 1 ? "y" : "ies",
           stats.total_files, stats.total_files == 1 ? "" : "s");

    /* ---- release heap memory owned by opts ---- */
    cli_opts_free(&opts);

    return 0;
}
