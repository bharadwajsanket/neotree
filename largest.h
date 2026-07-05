/*
 * largest.h -- top-N largest files and directories
 *
 * Two standalone, stdout-only display modes:
 *
 *   largest_files_print() -- traverse the tree, collect the N largest
 *     files by byte count, and print a sorted, aligned table to stdout.
 *
 *   largest_dirs_print() -- traverse the tree, compute the recursive
 *     byte total for every visited directory, and print the top N
 *     by size to stdout.
 *
 * Both functions:
 *   - respect opts->ignore / opts->show_all / opts->pattern / opts->max_depth
 *   - sort descending by size (stable: equal sizes sorted by path)
 *   - print clean, right-aligned columnar output
 *   - do NOT render the directory tree (standalone mode)
 *
 * Output formats:
 *
 *   Largest files (top N):
 *     #1   15.4 KB   tree.c
 *     #2   10.6 KB   main.c
 *
 *   Largest directories (top N):
 *     #1   66.0 KB    7 files   ./
 *     #2   44.0 KB    5 files   ./src
 */

#ifndef LARGEST_H
#define LARGEST_H

#include "cli.h"

/*
 * largest_files_print -- print the top `n` largest files found under
 * all given roots.
 *
 *   roots       -- array of root paths
 *   roots_count -- number of entries in roots[]
 *   opts        -- parsed CLI options (read-only); gitignore already loaded
 *   n           -- maximum number of results to print
 */
void largest_files_print(const char * const roots[], int roots_count,
                          const cli_opts_t *opts, int n);

/*
 * largest_dirs_print -- print the top `n` directories by recursive
 * byte total found under all given roots.
 *
 *   roots       -- array of root paths
 *   roots_count -- number of entries in roots[]
 *   opts        -- parsed CLI options (read-only); gitignore already loaded
 *   n           -- maximum number of results to print
 */
void largest_dirs_print(const char * const roots[], int roots_count,
                         const cli_opts_t *opts, int n);

#endif /* LARGEST_H */
