/*
 * tree.c — directory traversal and rendering
 *
 * Design:
 *   1. Open the directory once with fs_opendir / fs_readdir.
 *   2. Collect all visible entries into an entry_vec_t (heap array).
 *   3. Sort in place (dirs-first + alpha, or pure alpha).
 *   4. Render each entry with the right prefix characters.
 *   5. Recurse into sub-directories.
 *
 * This replaces the original two-pass approach (separate scandir calls
 * for count_files and print_tree) with a single pass per directory.
 *
 * Box-drawing characters used:
 *   ├── (U+251C U+2500 U+2500)  — non-last sibling
 *   └── (U+2514 U+2500 U+2500)  — last sibling
 *   │   (U+2502)                — continuing branch
 *       (four spaces)           — closed branch
 *
 * All box chars are UTF-8 encoded; they render correctly on any
 * modern terminal (Linux, macOS, Windows Terminal / WT).
 */

#include "tree.h"
#include "fs.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                           */
/* ------------------------------------------------------------------ */

#define MAX_PATH       4096
#define MAX_PREFIX     2048

/* Visual branch characters (UTF-8) */
#define BRANCH_MID     "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 "  /* ├── */
#define BRANCH_LAST    "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 "  /* └── */
#define BRANCH_VERT    "\xe2\x94\x82   "                         /* │   */
#define BRANCH_EMPTY   "    "

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/*
 * render_entry — print one line of the tree.
 *
 *   prefix   visual indent built from parent calls
 *   is_last  whether this entry is the last in its parent
 *   name     entry's filename
 *   is_dir   1 = directory
 *   n_files  relevant only for dirs: direct visible file count
 *            pass -1 to suppress the count annotation
 */
static void render_entry(const char *prefix,
                         int         is_last,
                         const char *name,
                         int         is_dir,
                         int         n_files) {
    const char *connector = is_last ? BRANCH_LAST : BRANCH_MID;
    const char *color     = color_for_name(name, is_dir);
    const char *reset     = color_reset();

    if (is_dir) {
        if (n_files == 0)
            printf("%s%s%s%s/%s  [empty]\n",
                   prefix, connector, color, name, reset);
        else if (n_files > 0)
            printf("%s%s%s%s/%s  (%d file%s)\n",
                   prefix, connector, color, name, reset,
                   n_files, n_files == 1 ? "" : "s");
        else
            /* n_files == -1: depth limit reached, skip annotation */
            printf("%s%s%s%s/%s\n",
                   prefix, connector, color, name, reset);
    } else {
        printf("%s%s%s%s%s\n",
               prefix, connector, color, name, reset);
    }
}

/*
 * count_direct_files — count direct file children of `path` that
 * are not ignored and match the pattern.
 *
 * Called once per directory so we can show "N files" in the dir line.
 * Because we already iterate below in tree_walk, this is an extra
 * iteration — but it's O(entries) and avoids the old double-scandir.
 * If you want to eliminate it entirely, pass -1 to render_entry.
 */
static int count_direct_files(const char *path,
                               const cli_opts_t *opts) {
    fs_dir_t  *dir = fs_opendir(path);
    if (!dir) return -1;

    fs_entry_t e;
    int count = 0;

    while (fs_readdir(dir, &e)) {
        if (e.type == FS_ENTRY_DIR)    continue;
        if (ignore_match(e.name, opts->ignore, opts->ignore_count)) continue;
        if (!pattern_match(e.name, opts->pattern)) continue;
        count++;
    }

    fs_closedir(dir);
    return count;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void tree_walk(const char       *path,
               const char       *prefix,
               const cli_opts_t *opts,
               int               current_depth,
               tree_stats_t     *stats) {

    /* Depth limit: stop before opening the directory */
    if (opts->max_depth != -1 && current_depth >= opts->max_depth)
        return;

    /* ---- 1. Collect all visible entries ---- */
    fs_dir_t *dir = fs_opendir(path);
    if (!dir) {
        fprintf(stderr, "neotree: cannot open '%s'\n", path);
        return;
    }

    entry_vec_t vec;
    entry_vec_init(&vec);

    fs_entry_t e;
    while (fs_readdir(dir, &e)) {
        /* skip ignored names */
        if (ignore_match(e.name, opts->ignore, opts->ignore_count))
            continue;

        int is_dir = (e.type == FS_ENTRY_DIR);

        /* for files: apply pattern filter */
        if (!is_dir && !pattern_match(e.name, opts->pattern))
            continue;

        if (!entry_vec_push(&vec, e.name, is_dir)) {
            fprintf(stderr, "neotree: out of memory\n");
            break;
        }
    }
    fs_closedir(dir);

    /* ---- 2. Sort ---- */
    entry_vec_sort(&vec, opts->dirs_first);

    /* ---- 3. Render + recurse ---- */
    for (int i = 0; i < vec.len; i++) {
        entry_t    *ent    = &vec.data[i];
        int         is_last = (i == vec.len - 1);

        char child_path[MAX_PATH];
        fs_join(child_path, sizeof(child_path), path, ent->name);

        if (ent->is_dir) {
            stats->total_dirs++;

            /*
             * Count direct visible files inside the dir so we can
             * display "N files" on the dir line.
             * If we're at max_depth - 1 we won't recurse, so just
             * pass -1 to suppress the annotation rather than lying.
             */
            int n_files = -1;
            int at_leaf = (opts->max_depth != -1 &&
                           current_depth + 1 >= opts->max_depth);
            if (!at_leaf)
                n_files = count_direct_files(child_path, opts);

            render_entry(prefix, is_last, ent->name, 1, n_files);

            /* Build new prefix for children */
            char new_prefix[MAX_PREFIX];
            snprintf(new_prefix, sizeof(new_prefix), "%s%s",
                     prefix,
                     is_last ? BRANCH_EMPTY : BRANCH_VERT);

            tree_walk(child_path, new_prefix, opts,
                      current_depth + 1, stats);

        } else {
            stats->total_files++;
            render_entry(prefix, is_last, ent->name, 0, 0);
        }
    }

    entry_vec_free(&vec);
}
