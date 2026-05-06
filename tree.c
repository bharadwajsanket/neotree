/*
 * tree.c -- directory traversal and rendering
 *
 * Design:
 *   1. Open the directory once with fs_opendir / fs_readdir.
 *   2. Collect all visible entries into an entry_vec_t.
 *   3. Sort in place (dirs-first + secondary key, or flat secondary key).
 *   4. Render each entry with the right prefix / connector characters.
 *   5. Recurse into sub-directories.
 *
 * Box-drawing characters (UTF-8):
 *   ├── (U+251C U+2500 U+2500)  non-last sibling
 *   └── (U+2514 U+2500 U+2500)  last sibling
 *   │   (U+2502)                continuing branch
 *       (four spaces)           closed branch
 *
 * Path-aware glob matching:
 *   tree_walk receives a rel_prefix string that tracks the relative
 *   path from the traversal root (e.g. "" at root, "src/" one level
 *   in, "src/internal/" two levels in).  When entry_is_visible tests
 *   a file against a ** pattern it constructs rel_path = rel_prefix +
 *   name and calls glob_match_path(rel_path, pattern).  This gives
 *   correct path-aware semantics: "src/STARSTAR/.h" will NOT match "cli.h".
 *
 * --dirs-only / is_last correctness:
 *   Files are collected but not rendered when --dirs-only is active.
 *   last_rendered_idx scans backward to find the last entry that will
 *   actually be printed, so BRANCH_LAST is placed correctly.
 *
 * Dual-stream output:
 *   render_entry writes to `out` (primary, may contain ANSI codes) and
 *   to `export_out` (plain text, no ANSI codes) in one pass.
 *   For markdown exports a second tree_walk pass is made from main.c.
 */

/* _DEFAULT_SOURCE for stat on glibc; must precede system headers */
#ifndef _WIN32
#  define _DEFAULT_SOURCE
#endif

#include "tree.h"
#include "fs.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Platform: file size and modification time                           */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
#  include <windows.h>

static long get_file_size_kb(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info))
        return -1;
    long long bytes = ((long long)info.nFileSizeHigh << 32) |
                      (unsigned long)info.nFileSizeLow;
    return (long)((bytes + 512) / 1024);
}

static long get_mtime(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info))
        return 0;
    ULARGE_INTEGER uli;
    uli.LowPart  = info.ftLastWriteTime.dwLowDateTime;
    uli.HighPart = info.ftLastWriteTime.dwHighDateTime;
    return (long)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

static int is_executable(const char *path) {
    (void)path;
    return 0;  /* Windows has no exec bits; colouring is by extension */
}

#else  /* POSIX */
#  include <sys/stat.h>

static long get_file_size_kb(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)((st.st_size + 512) / 1024);
}

static long get_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (long)st.st_mtime;
}

static int is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
}

#endif /* _WIN32 */

/* ------------------------------------------------------------------ */
/*  Constants                                                           */
/* ------------------------------------------------------------------ */

#define MAX_PATH       4096
#define MAX_PREFIX     2048
#define MAX_REL        4096   /* max length of relative path string */

/* Visual branch characters (UTF-8) */
#define BRANCH_MID     "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 "  /* ├── */
#define BRANCH_LAST    "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 "  /* └── */
#define BRANCH_VERT    "\xe2\x94\x82   "                         /* │   */
#define BRANCH_EMPTY   "    "

/* ------------------------------------------------------------------ */
/*  entry_is_visible                                                    */
/* ------------------------------------------------------------------ */

/*
 * entry_is_visible -- returns 1 if the entry should appear in the tree.
 *
 * Filter order (do not reorder):
 *   1. Ignore list  -- applies regardless of --all.
 *   2. Hidden files -- skipped unless --all is set.
 *   3. Pattern      -- for files only.
 *                      Simple (non-**) patterns: test against name.
 *                      ** patterns: test against full relative path
 *                        (rel_prefix + name) via glob_match_path().
 *                      Directories always pass to allow recursion.
 *
 * rel_prefix is the path from the traversal root to the current
 * directory, always ending in '/' (or "" at root).
 */
static int entry_is_visible(const char *name, int is_dir,
                             const char *rel_prefix,
                             const cli_opts_t *opts) {
    /* 1. Ignore list */
    if (ignore_match(name, opts->ignore, opts->ignore_count))
        return 0;

    /* 2. Hidden entries */
    if (!opts->show_all && is_hidden(name))
        return 0;

    /* 3. Pattern filter (files only) */
    if (!is_dir && opts->pattern) {
        if (pattern_has_doublestar(opts->pattern)) {
            /* Path-aware match: build "rel_prefix/name" and test */
            char rel_path[MAX_REL];
            int n = snprintf(rel_path, sizeof(rel_path), "%s%s",
                             rel_prefix, name);
            if (n < 0 || n >= (int)sizeof(rel_path))
                return 0;  /* path too long -- skip safely */
            if (!glob_match_path(rel_path, opts->pattern))
                return 0;
        } else {
            /* Simple pattern: match against filename only */
            if (!pattern_match(name, opts->pattern))
                return 0;
        }
    }
    /* Directories always pass the pattern filter */

    return 1;
}

/* ------------------------------------------------------------------ */
/*  Plain-text export helper                                            */
/* ------------------------------------------------------------------ */

static void fprint_clean(FILE *f,
                          const char *prefix,
                          const char *connector,
                          const char *name,
                          int         is_dir,
                          int         n_files,
                          long        size_kb) {
    if (!f) return;

    if (is_dir) {
        if (n_files == 0)
            fprintf(f, "%s%s%s/  [empty]\n", prefix, connector, name);
        else if (n_files > 0)
            fprintf(f, "%s%s%s/  (%d file%s)\n",
                    prefix, connector, name,
                    n_files, n_files == 1 ? "" : "s");
        else
            fprintf(f, "%s%s%s/\n", prefix, connector, name);
    } else {
        if (size_kb >= 0)
            fprintf(f, "%s%s%s  (%ld KB)\n", prefix, connector, name, size_kb);
        else
            fprintf(f, "%s%s%s\n", prefix, connector, name);
    }
}

/* ------------------------------------------------------------------ */
/*  render_entry                                                        */
/* ------------------------------------------------------------------ */

static void render_entry(const char *prefix,
                          int         is_last,
                          const char *name,
                          const char *full_path,
                          int         is_dir,
                          int         n_files,
                          long        size_kb,
                          FILE       *out,
                          FILE       *export_out) {
    const char *connector = is_last ? BRANCH_LAST : BRANCH_MID;

    const char *color;
    if (is_dir) {
        color = color_for_name(name, 1);
    } else if (full_path && is_executable(full_path)) {
        color = color_for_exec();
    } else {
        color = color_for_name(name, 0);
    }
    const char *reset = color_reset();

    if (is_dir) {
        if (n_files == 0)
            fprintf(out, "%s%s%s%s/%s  [empty]\n",
                    prefix, connector, color, name, reset);
        else if (n_files > 0)
            fprintf(out, "%s%s%s%s/%s  (%d file%s)\n",
                    prefix, connector, color, name, reset,
                    n_files, n_files == 1 ? "" : "s");
        else
            fprintf(out, "%s%s%s%s/%s\n",
                    prefix, connector, color, name, reset);
    } else {
        if (size_kb >= 0)
            fprintf(out, "%s%s%s%s%s  (%ld KB)\n",
                    prefix, connector, color, name, reset, size_kb);
        else
            fprintf(out, "%s%s%s%s%s\n",
                    prefix, connector, color, name, reset);
    }

    fprint_clean(export_out, prefix, connector, name, is_dir, n_files, size_kb);
}

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/*
 * count_direct_files -- count direct file children of path that pass
 * entry_is_visible().  Used for the "(N files)" directory annotation.
 *
 * Returns -1 on open failure.
 * Returns 0 when --dirs-only is active (annotation would mislead).
 */
static int count_direct_files(const char *path,
                               const char *rel_prefix,
                               const cli_opts_t *opts) {
    if (opts->dirs_only) return 0;

    fs_dir_t *dir = fs_opendir(path);
    if (!dir) return -1;

    fs_entry_t e;
    int count = 0;

    while (fs_readdir(dir, &e)) {
        if (e.type == FS_ENTRY_DIR) continue;
        if (!entry_is_visible(e.name, 0, rel_prefix, opts)) continue;
        count++;
    }

    fs_closedir(dir);
    return count;
}

/*
 * last_rendered_idx -- find the index of the last entry that will
 * actually be rendered (needed for correct BRANCH_LAST placement).
 */
static int last_rendered_idx(const entry_vec_t *vec, int dirs_only) {
    for (int i = vec->len - 1; i >= 0; i--) {
        if (!dirs_only || vec->data[i].is_dir)
            return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void tree_walk(const char       *path,
               const char       *tree_prefix,
               const char       *rel_prefix,
               const cli_opts_t *opts,
               int               current_depth,
               tree_stats_t     *stats,
               FILE             *out,
               FILE             *export_out,
               ext_table_t      *ext_tbl) {

    if (opts->max_depth != -1 && current_depth >= opts->max_depth)
        return;

    /* ---- 1. Collect visible entries ---- */
    fs_dir_t *dir = fs_opendir(path);
    if (!dir) {
        fprintf(stderr, "neotree: cannot open '%s'\n", path);
        return;
    }

    entry_vec_t vec;
    entry_vec_init(&vec);

    fs_entry_t e;
    while (fs_readdir(dir, &e)) {
        int is_dir = (e.type == FS_ENTRY_DIR);

        if (!entry_is_visible(e.name, is_dir, rel_prefix, opts))
            continue;

        long sz = -1, mt = 0;
        if (!is_dir) {
            char child_path[MAX_PATH];
            fs_join(child_path, sizeof(child_path), path, e.name);
            if (opts->sort_by == SORT_SIZE || opts->show_size)
                sz = get_file_size_kb(child_path);
            if (opts->sort_by == SORT_MODIFIED)
                mt = get_mtime(child_path);
        }

        if (!entry_vec_push(&vec, e.name, is_dir, sz, mt)) {
            fprintf(stderr, "neotree: out of memory\n");
            break;
        }
    }
    fs_closedir(dir);

    /* ---- 2. Sort ---- */
    entry_vec_sort(&vec, opts->dirs_first, opts->sort_by);

    /* ---- 3. Last rendered index ---- */
    int last_idx = last_rendered_idx(&vec, opts->dirs_only);

    /* ---- 4. Render + recurse ---- */
    for (int i = 0; i < vec.len; i++) {
        entry_t *ent = &vec.data[i];

        char child_path[MAX_PATH];
        fs_join(child_path, sizeof(child_path), path, ent->name);

        if (ent->is_dir) {
            stats->total_dirs++;

            int is_last = (i == last_idx);

            /* Build the relative path for this directory for sub-tree */
            char child_rel[MAX_REL];
            snprintf(child_rel, sizeof(child_rel), "%s%s/",
                     rel_prefix, ent->name);

            int n_files = -1;
            int at_leaf = (opts->max_depth != -1 &&
                           current_depth + 1 >= opts->max_depth);
            if (!at_leaf)
                n_files = count_direct_files(child_path, child_rel, opts);

            render_entry(tree_prefix, is_last, ent->name, NULL,
                         1, n_files, -1, out, export_out);

            char new_prefix[MAX_PREFIX];
            snprintf(new_prefix, sizeof(new_prefix), "%s%s",
                     tree_prefix,
                     is_last ? BRANCH_EMPTY : BRANCH_VERT);

            tree_walk(child_path, new_prefix, child_rel, opts,
                      current_depth + 1, stats,
                      out, export_out, ext_tbl);

        } else {
            stats->total_files++;

            if (ext_tbl)
                ext_table_add(ext_tbl, ent->name);

            if (!opts->dirs_only) {
                int is_last = (i == last_idx);

                long size_kb = -1;
                if (opts->show_size) {
                    size_kb = ent->size_kb >= 0
                              ? ent->size_kb
                              : get_file_size_kb(child_path);
                }

                render_entry(tree_prefix, is_last, ent->name, child_path,
                             0, 0, size_kb, out, export_out);
            }
        }
    }

    entry_vec_free(&vec);
}
