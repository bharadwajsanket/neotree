/*
 * find.c — isolated path search traversal
 *
 * Implements --find and --find-dir: a flat, exhaustive recursive walk
 * that outputs matching paths to stdout.  No tree rendering, no sorting,
 * no stats.  Hidden entries are visited (find is intentionally exhaustive).
 *
 * Ignore-list filtering IS applied to directories before recursion so that
 * find does not descend into .git/, node_modules/, or any user-specified
 * ignored entry.  This matches the expected mental model: "find files in
 * this project, excluding the same directories neotree normally skips."
 *
 * match_query() handles comma-separated queries and the same glob rules
 * used elsewhere: exact name, *.ext suffix, / path, ** path-aware globs.
 * Note: strtok() is used in match_query() — safe here because find_walk()
 * is single-threaded and never calls match_query() reentrantly.
 */

#include "find.h"
#include "fs.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int match_query(const char *basename, const char *rel_path, const char *query) {
    if (!query) return 0;

    /* query can be comma-separated, e.g. "a.c,b.c" */
    char qbuf[1024];
    strncpy(qbuf, query, sizeof(qbuf) - 1);
    qbuf[sizeof(qbuf) - 1] = '\0';

    char *tok = strtok(qbuf, ",");
    while (tok) {
        /* Strip leading and trailing spaces */
        while (*tok == ' ') tok++;
        size_t len = strlen(tok);
        while (len > 0 && tok[len - 1] == ' ') {
            tok[--len] = '\0';
        }

        if (tok[0] == '\0') {
            tok = strtok(NULL, ",");
            continue;
        }

        /* Check for leading slash anchor to root */
        if (tok[0] == '/') {
            const char *sub_query = tok + 1;
            if (pattern_has_doublestar(sub_query)) {
                if (glob_match_path(rel_path, sub_query)) return 1;
            } else {
                if (pattern_match(rel_path, sub_query)) return 1;
            }
        } else {
            /* If query contains '/', match against rel_path */
            if (strchr(tok, '/') != NULL) {
                if (pattern_has_doublestar(tok)) {
                    if (glob_match_path(rel_path, tok)) return 1;
                } else {
                    if (pattern_match(rel_path, tok)) return 1;
                }
            } else {
                /* Otherwise match against basename */
                if (pattern_match(basename, tok)) return 1;
            }
        }

        tok = strtok(NULL, ",");
    }

    return 0;
}

void find_walk(const char *dir_path, const char *rel_path, const cli_opts_t *opts, int current_depth) {
    if (opts->max_depth != -1 && current_depth >= opts->max_depth)
        return;

    fs_dir_t *dir = fs_opendir(dir_path);
    if (!dir) return;

    fs_entry_t e;
    while (fs_readdir(dir, &e)) {
        int is_dir = (e.type == FS_ENTRY_DIR);

        /* Construct child paths */
        char child_dir[4096];
        if (!fs_join(child_dir, sizeof(child_dir), dir_path, e.name)) {
            fprintf(stderr, "neotree: path too long, skipping: %s/%s\n", dir_path, e.name);
            continue;
        }

        char child_rel[4096];
        if (rel_path[0] == '\0') {
            snprintf(child_rel, sizeof(child_rel), "%s", e.name);
        } else {
            snprintf(child_rel, sizeof(child_rel), "%s/%s", rel_path, e.name);
        }

        if (is_dir) {
            /* Respect the ignore list — do not descend into ignored dirs */
            if (ignore_match(e.name, opts->ignore, opts->ignore_count))
                continue;

            /* Check if directory matches find_dir query */
            if (opts->find_dir) {
                if (match_query(e.name, child_rel, opts->find_dir)) {
                    printf("%s\n", child_dir);
                }
            }
            /* Recurse */
            find_walk(child_dir, child_rel, opts, current_depth + 1);
        } else {
            /* Check if file matches find query */
            if (opts->find) {
                if (match_query(e.name, child_rel, opts->find)) {
                    printf("%s\n", child_dir);
                }
            }
        }
    }

    fs_closedir(dir);
}
