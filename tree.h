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
 * Both a primary stream (stdout, with color) and optional export
 * streams (plain text, markdown, JSON) are supported simultaneously.
 *
 * ext_tbl (optional): if non-NULL, file extensions are recorded for
 * the --ext-summary output.
 *
 * Return value:
 *   tree_walk returns the total byte count of all visible files in the
 *   subtree.  This is used by the caller to track directory sizes for
 *   the --stats "largest dir" field.  When --stats is not active the
 *   return value is 0.
 */

#include <stdio.h>
#include "cli.h"
#include "utils.h"

/* ------------------------------------------------------------------ */

typedef struct {
    int total_files;
    int total_dirs;
    long long total_size_bytes;

    /* Largest file by byte count */
    char      largest_file[256];
    long long largest_file_size_bytes;

    /* Largest directory by recursive byte count (populated only when
     * show_stats is active; path is relative to the traversal root) */
    char      largest_dir[4096];
    long long largest_dir_size_bytes;

    /* Deepest entry seen (relative path from traversal root) */
    char deepest_path[4096];
    int  max_depth;
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
 *   export_txt    secondary plain-text stream; NULL if not exporting
 *   export_md     markdown stream; NULL if not exporting
 *   export_json   JSON stream; NULL if not exporting
 *   ext_tbl       extension table for --ext-summary; NULL if unused
 *
 * Returns the total byte count of all visible files in this subtree.
 * Only meaningful when opts->show_stats is true; returns 0 otherwise.
 */
long long tree_walk(const char       *path,
                    const char       *tree_prefix,
                    const char       *rel_prefix,
                    const cli_opts_t *opts,
                    int               current_depth,
                    tree_stats_t     *stats,
                    FILE             *out,
                    FILE             *export_txt,
                    FILE             *export_md,
                    FILE             *export_json,
                    ext_table_t      *ext_tbl);

#endif /* TREE_H */
