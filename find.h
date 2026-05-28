#ifndef FIND_H
#define FIND_H

/*
 * find.h — isolated path search traversal
 *
 * find_walk() performs an exhaustive recursive directory walk,
 * printing matching paths to stdout.  Used by --find and --find-dir.
 * The ignore list from opts is respected; hidden entries are not filtered.
 */

#include "cli.h"

void find_walk(const char *dir_path, const char *rel_path,
               const cli_opts_t *opts, int current_depth);

#endif /* FIND_H */
