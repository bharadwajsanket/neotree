/*
 * tree.c -- directory traversal and rendering
 *
 * Design:
 *   1. Open the directory once with fs_opendir / fs_readdir.
 *   2. Collect all visible entries into an entry_vec_t.
 *   3. Sort in place (dirs-first + secondary key, or flat secondary key).
 *   4. Pre-compute max size-string width for aligned --size output.
 *   5. Render each entry with the right prefix / connector characters.
 *      When stdout is a TTY, long filenames are truncated with '…' to
 *      prevent wrapping and layout jitter.
 *   6. Recurse into sub-directories, accumulating subtree byte count.
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
 *   to `export_txt` / `export_md` (plain text, no ANSI codes) in one pass.
 *
 * Size alignment:
 *   Before the render loop, we scan the entry_vec_t for the maximum
 *   format_size() string width among visible siblings, so all size
 *   annotations in a directory are right-aligned to a uniform column.
 *
 * Directory size tracking (for --stats):
 *   tree_walk returns its subtree byte total.  The parent call
 *   compares each child total against stats->largest_dir_size_bytes
 *   and updates stats->largest_dir when a new maximum is found.
 *
 * Terminal width / filename truncation:
 *   When stdout is a TTY, get_terminal_width() returns the window
 *   width.  The render_entry helper computes the available columns for
 *   the filename (total width minus prefix, connector, and size field)
 *   and calls utf8_truncate() when the name would overflow.
 *   Truncation is disabled when stdout is piped (width == 0).
 *
 * Unified JSON Export:
 *   JSON export is built directly into tree_walk.  It runs in a single
 *   pass alongside stdout/txt/md rendering, preventing duplicate
 *   traversals, visibility checks, ignore matches, and sorting.
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

/* Visual column width of each branch unit (BRANCH_VERT / BRANCH_EMPTY) */
#define BRANCH_UNIT_COLS 4
/* Visual column width of BRANCH_MID / BRANCH_LAST connectors */
#define CONNECTOR_COLS   4

/* ------------------------------------------------------------------ */
/*  JSON formatting helpers                                             */
/* ------------------------------------------------------------------ */

static void jindent(FILE *out, int depth) {
    int i;
    for (i = 0; i < depth * 2; i++)
        fputc(' ', out);
}

static void json_escape(FILE *out, const char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\b': fputs("\\b",  out); break;
        case '\f': fputs("\\f",  out); break;
        case '\n': fputs("\\n",  out); break;
        case '\r': fputs("\\r",  out); break;
        case '\t': fputs("\\t",  out); break;
        default:
            if (c < 0x20) {
                /* Other control characters: \uXXXX */
                fprintf(out, "\\u%04x", (unsigned int)c);
            } else {
                fputc((int)c, out);
            }
            break;
        }
    }
}

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
static int entry_is_visible(const char *name, int is_hidden, int is_dir,
                             const char *rel_prefix,
                             const cli_opts_t *opts) {
    /* 1. Ignore list */
    if (ignore_match(name, opts->ignore, opts->ignore_count))
        return 0;

    /* 2. Hidden entries */
    if (!opts->show_all && is_hidden)
        return 0;

    /* 3. Pattern filter (files only) */
    if (!is_dir && opts->pattern) {
        /* Path-aware match: build "rel_prefix/name" and test */
        char rel_path[MAX_REL];
        int n = snprintf(rel_path, sizeof(rel_path), "%s%s",
                         rel_prefix, name);
        if (n < 0 || n >= (int)sizeof(rel_path))
            return 0;  /* path too long -- skip safely */
        if (!glob_match_path(rel_path, opts->pattern))
            return 0;
    }
    /* Directories always pass the pattern filter */

    return 1;
}

/* ------------------------------------------------------------------ */
/*  Visual prefix width helper                                          */
/* ------------------------------------------------------------------ */

/*
 * visual_prefix_cols -- count the visual column width of a tree prefix.
 *
 * Each "unit" in tree_prefix is either BRANCH_VERT (7 bytes, 4 visual
 * columns) or BRANCH_EMPTY (4 bytes, 4 visual columns).  We advance
 * through the string in unit-sized jumps.
 */
static int visual_prefix_cols(const char *prefix) {
    int cols = 0;
    while (*prefix) {
        unsigned char c = (unsigned char)*prefix;
        if (c == 0xE2) {
            /* BRANCH_VERT: 3-byte UTF-8 lead + 3 spaces = 7 bytes */
            prefix += 7;
        } else if (c == ' ') {
            /* BRANCH_EMPTY: 4 spaces */
            prefix += 4;
        } else {
            break; /* unexpected -- stop */
        }
        cols += BRANCH_UNIT_COLS;
    }
    return cols;
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
                          long long   size_bytes,
                          int         size_field_width) {
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
        if (size_field_width > 0 && size_bytes >= 0) {
            char sz[32];
            format_size(size_bytes, sz, sizeof(sz));
            fprintf(f, "%s%s%s  %*s\n",
                    prefix, connector, name, size_field_width, sz);
        } else {
            fprintf(f, "%s%s%s\n", prefix, connector, name);
        }
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
                          long long   size_bytes,
                          int         size_field_width,
                          FILE       *out,
                          FILE       *export_txt,
                          FILE       *export_md) {
    const char *connector = is_last ? BRANCH_LAST : BRANCH_MID;

    const char *color;
    if (is_dir) {
        color = color_for_name(name, 1);
    } else if (full_path && fs_is_exec(full_path)) {
        color = color_for_exec();
    } else {
        color = color_for_name(name, 0);
    }
    const char *reset = color_reset();

    /* Terminal width truncation (TTY only) */
    const char *display_name = name;
    char truncated[512];
    {
        int tw = get_terminal_width();
        if (tw > 0) {
            int prefix_vis = visual_prefix_cols(prefix);
            int size_vis   = (size_field_width > 0) ? (size_field_width + 2) : 0;
            int dir_mark   = is_dir ? 1 : 0; /* trailing '/' */
            int available  = tw - prefix_vis - CONNECTOR_COLS - size_vis - dir_mark;
            if (available > 3 && (int)strlen(name) > available) {
                if (utf8_truncate(truncated, sizeof(truncated), name, available))
                    display_name = truncated;
            }
        }
    }

    if (is_dir) {
        if (out) {
            if (n_files == 0)
                fprintf(out, "%s%s%s%s/%s  [empty]\n",
                        prefix, connector, color, display_name, reset);
            else if (n_files > 0)
                fprintf(out, "%s%s%s%s/%s  (%d file%s)\n",
                        prefix, connector, color, display_name, reset,
                        n_files, n_files == 1 ? "" : "s");
            else
                fprintf(out, "%s%s%s%s/%s\n",
                        prefix, connector, color, display_name, reset);
        }
    } else {
        if (out) {
            if (size_field_width > 0 && size_bytes >= 0) {
                char sz[32];
                format_size(size_bytes, sz, sizeof(sz));
                fprintf(out, "%s%s%s%s%s  %*s\n",
                        prefix, connector, color, display_name, reset,
                        size_field_width, sz);
            } else {
                fprintf(out, "%s%s%s%s%s\n",
                        prefix, connector, color, display_name, reset);
            }
        }
    }

    fprint_clean(export_txt, prefix, connector, name, is_dir, n_files,
                 size_bytes, size_field_width);
    fprint_clean(export_md,  prefix, connector, name, is_dir, n_files,
                 size_bytes, size_field_width);
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
    fs_dir_t *dir;
    fs_entry_t e;
    int count;

    if (opts->dirs_only) return 0;

    dir = fs_opendir(path);
    if (!dir) return -1;

    count = 0;
    while (fs_readdir(dir, &e)) {
        if (e.type == FS_ENTRY_DIR) continue;
        if (!entry_is_visible(e.name, e.is_hidden, 0, rel_prefix, opts)) continue;
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
    int i;
    for (i = vec->len - 1; i >= 0; i--) {
        if (!dirs_only || vec->data[i].is_dir)
            return i;
    }
    return -1;
}

/*
 * max_size_field_width -- scan visible file siblings in vec and return
 * the maximum formatted-size string width.  Returns 0 when --size is
 * off or when there are no files with known sizes.
 */
static int max_size_field_width(const entry_vec_t *vec, int show_size) {
    int max_w = 0;
    int i;
    if (!show_size) return 0;
    for (i = 0; i < vec->len; i++) {
        if (!vec->data[i].is_dir && vec->data[i].size_bytes >= 0) {
            char sz[32];
            int w;
            format_size(vec->data[i].size_bytes, sz, sizeof(sz));
            w = (int)strlen(sz);
            if (w > max_w) max_w = w;
        }
    }
    return max_w;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

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
                    ext_table_t      *ext_tbl) {
    int i;
    long long subtree_bytes = 0LL;
    int json_child_count = 0;
    int base_indent = (opts->roots_count > 1) ? 1 : 0;
    int node_indent = base_indent + 2 + current_depth * 2;

    if (opts->max_depth != -1 && current_depth >= opts->max_depth)
        return 0LL;

    /* ---- JSON Root Start ---- */
    if (export_json && current_depth == 0) {
        const char *root_name = utils_basename(path);
        if (!root_name || root_name[0] == '\0') root_name = path;

        jindent(export_json, base_indent);
        fputs("{\n", export_json);

        jindent(export_json, base_indent + 1);
        fputs("\"name\": \"", export_json);
        json_escape(export_json, root_name);
        fputs("\",\n", export_json);

        jindent(export_json, base_indent + 1);
        fputs("\"path\": \"", export_json);
        json_escape(export_json, path);
        fputs("\",\n", export_json);

        jindent(export_json, base_indent + 1);
        fputs("\"type\": \"dir\",\n", export_json);

        jindent(export_json, base_indent + 1);
        fputs("\"size\": -1,\n", export_json);

        jindent(export_json, base_indent + 1);
        fputs("\"mtime\": 0,\n", export_json);

        jindent(export_json, base_indent + 1);
        fputs("\"children\": [", export_json);
    }

    /* ---- 1. Collect visible entries ---- */
    {
        fs_dir_t *dir = fs_opendir(path);
        if (!dir) {
            fprintf(stderr, "neotree: cannot open '%s'\n", path);
            if (export_json && current_depth == 0) {
                fputc('\n', export_json);
                jindent(export_json, base_indent + 1);
                fputs("]\n", export_json);
                jindent(export_json, base_indent);
                fputc('}', export_json);
            }
            return 0LL;
        }

        entry_vec_t vec;
        entry_vec_init(&vec);

        {
            fs_entry_t e;
            while (fs_readdir(dir, &e)) {
                int is_dir = (e.type == FS_ENTRY_DIR);
                long long sz = -1LL;
                long long mt = 0LL;

                if (!entry_is_visible(e.name, e.is_hidden, is_dir, rel_prefix, opts))
                    continue;

                if (!is_dir) {
                    char child_path[MAX_PATH];
                    if (fs_join(child_path, sizeof(child_path), path, e.name)) {
                        /* Fetch size when needed for sort, display, or stats */
                        if (opts->sort_by == SORT_SIZE || opts->show_size || opts->show_stats || export_json)
                            sz = fs_get_size(child_path);
                        if (opts->sort_by == SORT_MODIFIED || export_json)
                            mt = fs_get_mtime(child_path);
                    }
                }

                if (!entry_vec_push(&vec, e.name, is_dir, sz, mt)) {
                    fprintf(stderr, "neotree: out of memory\n");
                    break;
                }
            }
        }
        fs_closedir(dir);

        /* ---- 2. Sort ---- */
        entry_vec_sort(&vec, opts->dirs_first, opts->sort_by, opts->reverse);

        /* ---- 3. Pre-compute size column width for alignment ---- */
        int sz_width = max_size_field_width(&vec, opts->show_size);

        /* ---- 4. Last rendered index ---- */
        int last_idx = last_rendered_idx(&vec, opts->dirs_only);

        /* ---- 5. Render + recurse ---- */
        for (i = 0; i < vec.len; i++) {
            entry_t *ent = &vec.data[i];
            char child_path[MAX_PATH];

            if (!fs_join(child_path, sizeof(child_path), path, ent->name)) {
                fprintf(stderr, "neotree: path too long, skipping: %s/%s\n",
                        path, ent->name);
                continue;
            }

            if (ent->is_dir) {
                int is_last;
                char child_rel[MAX_REL];
                char child_rel_dir[MAX_REL];
                char new_prefix[MAX_PREFIX];
                int n_files;
                int at_leaf;
                long long dir_bytes;

                stats->total_dirs++;

                /* Track deepest point seen */
                if (current_depth + 1 > stats->max_depth) {
                    stats->max_depth = current_depth + 1;
                    {
                        char dp[MAX_REL];
                        snprintf(dp, sizeof(dp), "%s%s", rel_prefix, ent->name);
                        strncpy(stats->deepest_path, dp,
                                sizeof(stats->deepest_path) - 1);
                        stats->deepest_path[sizeof(stats->deepest_path) - 1] = '\0';
                    }
                }

                is_last = (i == last_idx);

                snprintf(child_rel, sizeof(child_rel), "%s%s",
                         rel_prefix, ent->name);
                snprintf(child_rel_dir, sizeof(child_rel_dir), "%s%s/",
                         rel_prefix, ent->name);

                at_leaf = (opts->max_depth != -1 &&
                           current_depth + 1 >= opts->max_depth);
                n_files = at_leaf ? -1
                                  : count_direct_files(child_path, child_rel_dir, opts);

                /* ---- Render visual / plain exports ---- */
                render_entry(tree_prefix, is_last, ent->name, NULL,
                             1, n_files, -1LL, 0,
                             out, export_txt, export_md);

                /* ---- Render JSON entry start ---- */
                if (export_json) {
                    if (json_child_count > 0) fputs(",\n", export_json);
                    else fputc('\n', export_json);
                    json_child_count++;

                    jindent(export_json, node_indent);
                    fputs("{\n", export_json);

                    jindent(export_json, node_indent + 1);
                    fputs("\"name\": \"", export_json);
                    json_escape(export_json, ent->name);
                    fputs("\",\n", export_json);

                    jindent(export_json, node_indent + 1);
                    fputs("\"path\": \"", export_json);
                    json_escape(export_json, child_rel);
                    fputs("\",\n", export_json);

                    jindent(export_json, node_indent + 1);
                    fputs("\"type\": \"dir\",\n", export_json);

                    jindent(export_json, node_indent + 1);
                    fputs("\"size\": -1,\n", export_json);

                    jindent(export_json, node_indent + 1);
                    fputs("\"mtime\": 0,\n", export_json);

                    jindent(export_json, node_indent + 1);
                    fputs("\"children\": [", export_json);
                }

                snprintf(new_prefix, sizeof(new_prefix), "%s%s",
                         tree_prefix,
                         is_last ? BRANCH_EMPTY : BRANCH_VERT);

                dir_bytes = tree_walk(child_path, new_prefix, child_rel_dir, opts,
                                      current_depth + 1, stats,
                                      out, export_txt, export_md, export_json, ext_tbl);
                subtree_bytes += dir_bytes;

                /* ---- Render JSON entry end ---- */
                if (export_json) {
                    fputc('\n', export_json);
                    jindent(export_json, node_indent + 1);
                    fputs("]\n", export_json);
                    jindent(export_json, node_indent);
                    fputc('}', export_json);
                }

                /* Track largest directory by recursive byte total */
                if (opts->show_stats && dir_bytes > stats->largest_dir_size_bytes) {
                    stats->largest_dir_size_bytes = dir_bytes;
                    strncpy(stats->largest_dir, child_rel_dir,
                            sizeof(stats->largest_dir) - 1);
                    stats->largest_dir[sizeof(stats->largest_dir) - 1] = '\0';
                    /* Strip trailing '/' for display */
                    {
                        size_t dlen = strlen(stats->largest_dir);
                        if (dlen > 1 && stats->largest_dir[dlen - 1] == '/')
                            stats->largest_dir[dlen - 1] = '\0';
                    }
                }

            } else {
                /* ---- File entry ---- */
                stats->total_files++;

                if (ext_tbl)
                    ext_table_add(ext_tbl, ent->name);

                if (!opts->dirs_only) {
                    int is_last = (i == last_idx);
                    long long size_bytes = ent->size_bytes; /* already fetched */

                    if (opts->show_stats && size_bytes >= 0) {
                        stats->total_size_bytes += size_bytes;
                        subtree_bytes           += size_bytes;
                        if (size_bytes > stats->largest_file_size_bytes) {
                            stats->largest_file_size_bytes = size_bytes;
                            strncpy(stats->largest_file, ent->name,
                                    sizeof(stats->largest_file) - 1);
                            stats->largest_file[sizeof(stats->largest_file) - 1] = '\0';
                        }
                    } else if (opts->show_stats) {
                        /* size_bytes was -1 (fetch failed); don't count */
                    }

                    /* Track deepest point seen */
                    if (current_depth + 1 > stats->max_depth) {
                        stats->max_depth = current_depth + 1;
                        {
                            char dp[MAX_REL];
                            snprintf(dp, sizeof(dp), "%s%s",
                                     rel_prefix, ent->name);
                            strncpy(stats->deepest_path, dp,
                                    sizeof(stats->deepest_path) - 1);
                            stats->deepest_path[sizeof(stats->deepest_path) - 1] = '\0';
                        }
                    }

                    /* Display size: pass -1 when --size is off */
                    {
                        long long display_sz = (opts->show_size && size_bytes >= 0)
                                                ? size_bytes : -1LL;
                        int display_width    = (opts->show_size) ? sz_width : 0;

                        render_entry(tree_prefix, is_last, ent->name, child_path,
                                     0, 0, display_sz, display_width,
                                     out, export_txt, export_md);
                    }

                    /* ---- Render JSON file entry ---- */
                    if (export_json) {
                        if (json_child_count > 0) fputs(",\n", export_json);
                        else fputc('\n', export_json);
                        json_child_count++;

                        jindent(export_json, node_indent);
                        fputs("{\n", export_json);

                        jindent(export_json, node_indent + 1);
                        fputs("\"name\": \"", export_json);
                        json_escape(export_json, ent->name);
                        fputs("\",\n", export_json);

                        jindent(export_json, node_indent + 1);
                        fputs("\"path\": \"", export_json);
                        char child_rel[MAX_REL];
                        snprintf(child_rel, sizeof(child_rel), "%s%s", rel_prefix, ent->name);
                        json_escape(export_json, child_rel);
                        fputs("\",\n", export_json);

                        jindent(export_json, node_indent + 1);
                        fputs("\"type\": \"file\",\n", export_json);

                        jindent(export_json, node_indent + 1);
                        if (size_bytes >= 0)
                            fprintf(export_json, "\"size\": %lld,\n", size_bytes);
                        else
                            fputs("\"size\": -1,\n", export_json);

                        jindent(export_json, node_indent + 1);
                        fprintf(export_json, "\"mtime\": %lld,\n", ent->mtime);

                        jindent(export_json, node_indent + 1);
                        fputs("\"children\": null\n", export_json);

                        jindent(export_json, node_indent);
                        fputc('}', export_json);
                    }
                } else {
                    /* dirs_only: still count subtree bytes for stats */
                    if (opts->show_stats && ent->size_bytes >= 0)
                        subtree_bytes += ent->size_bytes;
                }
            }
        }

        entry_vec_free(&vec);
    }

    /* ---- JSON Root End ---- */
    if (export_json && current_depth == 0) {
        fputc('\n', export_json);
        jindent(export_json, base_indent + 1);
        fputs("]\n", export_json);
        jindent(export_json, base_indent);
        fputc('}', export_json);
    }

    return subtree_bytes;
}
