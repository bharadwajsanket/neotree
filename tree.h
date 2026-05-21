#ifndef TREE_H
#define TREE_H

/*
 * tree.h -- directory traversal and rendering
 *
 * tree_walk does a single-pass recursive traversal:
 *   1. Open directory with fs_opendir / fs_readdir.
 *   2. Collect visible entries into an entry_vec_t.
 *   3. Sort in place.
 *   4. Render each entry.
 *   5. Recurse into sub-directories.
 *
 * The `rel_prefix` argument tracks the path relative to the traversal
 * root (e.g. "src/internal/") and is used by glob_match_path() so that
 * patterns like "src/STARSTAR/.h" correctly exclude files outside of src/.
 *
 * Both a primary stream (stdout, with color) and an optional export
 * stream (plain text, no ANSI codes) are supported simultaneously.
 *
 * ext_tbl (optional): if non-NULL, file extensions are recorded for
 * the --ext-summary output.
 */

#include <stdio.h>
#include "cli.h"
#include "utils.h"

/* ------------------------------------------------------------------ */

typedef struct {
    int total_files;
    int total_dirs;
} tree_stats_t;

/*
 * tree_walk -- render path recursively.
 *
 *   path          absolute (or relative) path to the current directory
 *   tree_prefix   visual indent prefix built from parent calls
 *                 (e.g. "|   |   ")
 *   rel_prefix    path relative to the original root, used for glob
 *                 matching (e.g. "" at root, "src/" one level in)
 *   opts          parsed CLI options (read-only)
 *   current_depth depth relative to root (root = 0)
 *   stats         accumulates file/dir counts; caller zero-initialises
 *   out           primary output stream (stdout, receives ANSI codes)
 *   export_out    secondary plain-text stream; NULL if not exporting
 *   ext_tbl       extension table for --ext-summary; NULL if unused
 */
void tree_walk(const char       *path,
               const char       *tree_prefix,
               const char       *rel_prefix,
               const cli_opts_t *opts,
               int               current_depth,
               tree_stats_t     *stats,
               FILE             *out,
               FILE             *export_out,
               ext_table_t      *ext_tbl);

#endif /* TREE_H */
