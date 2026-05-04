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
 * Box-drawing characters used:
 *   ├── (U+251C U+2500 U+2500)  — non-last sibling
 *   └── (U+2514 U+2500 U+2500)  — last sibling
 *   │   (U+2502)                — continuing branch
 *       (four spaces)           — closed branch
 *
 * All box chars are UTF-8 encoded; they render correctly on any
 * modern terminal (Linux, macOS, Windows Terminal / WT).
 *
 * Features:
 *   --all        : show hidden entries (name starts with '.')
 *   --dirs-only  : skip printing files; still recurse into dirs
 *   --size       : append "  (N KB)" to file lines using stat()
 *
 * --dirs-only / is_last correctness
 * ----------------------------------
 * When --dirs-only is active, files are collected into the vec (so that
 * sort order and stats remain correct) but are not rendered.  The
 * is_last connector must therefore be based on the index of the last
 * entry that will *actually be rendered*, not the last entry in vec.
 *
 * We compute this with last_rendered_idx: scan backwards from the end
 * of vec to find the last entry that is either a dir, or a file when
 * --dirs-only is off.  Entries before that index that happen to be
 * files get BRANCH_MID (they're still in the vec, just invisible),
 * which keeps the prefix stack consistent.
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
/*  Platform: file size                                                 */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
#  include <windows.h>

/*
 * get_file_size_kb — return file size rounded to KB, or -1 on error.
 * Windows: GetFileAttributesEx provides size without a full stat().
 */
static long get_file_size_kb(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info))
        return -1;
    /* Combine high and low 32-bit parts */
    long long bytes = ((long long)info.nFileSizeHigh << 32) |
                      (unsigned long)info.nFileSizeLow;
    return (long)((bytes + 512) / 1024);
}

#else  /* POSIX */
#  include <sys/stat.h>

/*
 * get_file_size_kb — return file size rounded to nearest KB, or -1 on
 * error (e.g. permission denied, broken symlink).
 * Uses stat() so symlink targets are followed; consistent with the size
 * a reader would actually see.
 */
static long get_file_size_kb(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)((st.st_size + 512) / 1024);
}

#endif /* _WIN32 */

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
/*  entry_is_visible — single, authoritative visibility gate           */
/* ------------------------------------------------------------------ */

/*
 * entry_is_visible — returns 1 if the entry should appear in the tree.
 *
 * Called identically in the collection loop (tree_walk) and the
 * counting helper (count_direct_files) so behaviour is always
 * consistent.
 *
 * Filter order (mandatory — do not reorder):
 *   1. Ignore list  — applies regardless of --all.
 *      .gitignore rules take effect here too.
 *   2. Hidden files — skipped unless --all is set.
 *      Note: step 1 already catches ignored dot-entries, so a
 *      .gitignore entry for e.g. ".DS_Store" works even with --all.
 *   3. Pattern      — file-only; directories always pass.
 */
static int entry_is_visible(const char *name, int is_dir,
                             const cli_opts_t *opts) {
    /* 1. Ignore list (CLI --ignore + .gitignore merged) */
    if (ignore_match(name, opts->ignore, opts->ignore_count))
        return 0;

    /* 2. Hidden entries (dot-files / dot-dirs) */
    if (!opts->show_all && is_hidden(name))
        return 0;

    /* 3. Pattern filter — directories are never filtered by pattern */
    if (!is_dir && !pattern_match(name, opts->pattern))
        return 0;

    return 1;
}

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/*
 * render_entry — print one line of the tree.
 *
 *   prefix    visual indent built from parent calls
 *   is_last   whether this is the last *rendered* entry in its parent
 *   name      entry's filename
 *   is_dir    1 = directory
 *   n_files   dirs only: direct visible file count (-1 → suppress annotation)
 *   size_kb   files only: size in KB (-1 → size not shown)
 */
static void render_entry(const char *prefix,
                         int         is_last,
                         const char *name,
                         int         is_dir,
                         int         n_files,
                         long        size_kb) {
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
            /* n_files == -1: suppress annotation */
            printf("%s%s%s%s/%s\n",
                   prefix, connector, color, name, reset);
    } else {
        if (size_kb >= 0)
            printf("%s%s%s%s%s  (%ld KB)\n",
                   prefix, connector, color, name, reset, size_kb);
        else
            printf("%s%s%s%s%s\n",
                   prefix, connector, color, name, reset);
    }
}

/*
 * count_direct_files — count direct file children of `path` that pass
 * entry_is_visible().
 *
 * Returns -1 on open failure (permission denied, etc.).
 * Returns 0 when --dirs-only is active: no files will be shown, so the
 * annotation would be misleading.
 *
 * Uses the same entry_is_visible() predicate as tree_walk so the count
 * always matches what is actually rendered.
 */
static int count_direct_files(const char *path, const cli_opts_t *opts) {
    /* When --dirs-only is active, files aren't shown → don't annotate */
    if (opts->dirs_only) return 0;

    fs_dir_t *dir = fs_opendir(path);
    if (!dir) return -1;

    fs_entry_t e;
    int count = 0;

    while (fs_readdir(dir, &e)) {
        if (e.type == FS_ENTRY_DIR) continue;       /* only count files */
        if (!entry_is_visible(e.name, 0, opts)) continue;
        count++;
    }

    fs_closedir(dir);
    return count;
}

/*
 * last_rendered_idx — find the index of the last entry in vec that will
 * actually be printed.
 *
 * When --dirs-only is off, this is simply vec.len - 1.
 * When --dirs-only is on, files are collected but not printed; the last
 * *printed* entry is the last directory in the vec.
 *
 * Returns -1 if nothing will be rendered (all entries are files and
 * --dirs-only is active).
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
        /* Permission denied or other error — report and skip subtree */
        fprintf(stderr, "neotree: cannot open '%s'\n", path);
        return;
    }

    entry_vec_t vec;
    entry_vec_init(&vec);

    fs_entry_t e;
    while (fs_readdir(dir, &e)) {
        int is_dir = (e.type == FS_ENTRY_DIR);

        if (!entry_is_visible(e.name, is_dir, opts))
            continue;

        if (!entry_vec_push(&vec, e.name, is_dir)) {
            fprintf(stderr, "neotree: out of memory\n");
            break;
        }
    }
    fs_closedir(dir);

    /* ---- 2. Sort ---- */
    entry_vec_sort(&vec, opts->dirs_first);

    /* ---- 3. Determine last rendered index for correct connectors ---- */
    int last_idx = last_rendered_idx(&vec, opts->dirs_only);

    /* ---- 4. Render + recurse ---- */
    for (int i = 0; i < vec.len; i++) {
        entry_t *ent = &vec.data[i];

        char child_path[MAX_PATH];
        fs_join(child_path, sizeof(child_path), path, ent->name);

        if (ent->is_dir) {
            stats->total_dirs++;

            /*
             * is_last must reflect whether this is the last *rendered*
             * entry, not just the last entry in vec.  With --dirs-only,
             * trailing files in vec are invisible, so the last directory
             * should receive BRANCH_LAST even if files follow it in vec.
             */
            int is_last = (i == last_idx);

            /*
             * Count direct visible files inside this dir for the
             * annotation.  Suppress if at the depth leaf (won't recurse)
             * or if --dirs-only (no files shown → annotation misleads).
             */
            int n_files = -1;
            int at_leaf = (opts->max_depth != -1 &&
                           current_depth + 1 >= opts->max_depth);
            if (!at_leaf)
                n_files = count_direct_files(child_path, opts);

            render_entry(prefix, is_last, ent->name, 1, n_files, -1);

            /* Build the prefix continuation for children */
            char new_prefix[MAX_PREFIX];
            snprintf(new_prefix, sizeof(new_prefix), "%s%s",
                     prefix,
                     is_last ? BRANCH_EMPTY : BRANCH_VERT);

            tree_walk(child_path, new_prefix, opts,
                      current_depth + 1, stats);

        } else {
            /* Always count files regardless of --dirs-only */
            stats->total_files++;

            if (!opts->dirs_only) {
                int is_last = (i == last_idx);

                long size_kb = -1;
                if (opts->show_size)
                    size_kb = get_file_size_kb(child_path);

                render_entry(prefix, is_last, ent->name, 0, 0, size_kb);
            }
        }
    }

    entry_vec_free(&vec);
}
