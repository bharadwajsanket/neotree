#ifndef TREE_H
#define TREE_H

/*
 * tree.h — directory traversal and rendering
 *
 * The tree_walk function does a single-pass recursive traversal.
 * It collects all entries in one scandir-equivalent call, sorts them,
 * then renders + recurses — so each directory is read exactly once.
 *
 * Summary stats (total files + dirs) are accumulated in tree_stats_t
 * and printed by the caller after traversal.
 */

#include "cli.h"

/* ------------------------------------------------------------------ */

typedef struct {
    int total_files;
    int total_dirs;
} tree_stats_t;

/*
 * tree_walk — render `path` recursively.
 *
 *   path          absolute or relative path to current node
 *   prefix        the visual prefix string built up as we descend
 *                 (e.g. "│   │   ")
 *   opts          parsed CLI options (read-only)
 *   current_depth depth of `path` relative to root (root = 0)
 *   stats         accumulates file/dir counts; caller zero-initialises
 */
void tree_walk(const char    *path,
               const char    *prefix,
               const cli_opts_t *opts,
               int            current_depth,
               tree_stats_t  *stats);

#endif /* TREE_H */
