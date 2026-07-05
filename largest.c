/*
 * largest.c -- top-N largest files and directories implementation
 *
 * Design:
 *
 *   File collection:
 *     lf_walk() recursively traverses directories, applying the same
 *     visibility rules as tree.c.  All matching files are collected into
 *     a dynamically growing array of (path, size_bytes) records.  After
 *     the full walk, the array is qsort()ed and the top N entries are
 *     printed.
 *
 *     Memory: O(total visible files).  For trees with millions of files
 *     this could be large; a min-heap would cap it to O(N).  For the
 *     practical use case (developer project trees) a flat array plus
 *     qsort is simpler and fast enough.
 *
 *   Directory collection:
 *     ld_walk() traversively traverses directories, accumulates file
 *     sizes into per-directory totals, and collects one record per
 *     directory.  The root itself is also recorded (total of everything
 *     under it).  After the walk the array is sorted and top N printed.
 *
 *   Both walkers reuse the same visibility check as tree.c.
 *
 *   Output alignment:
 *     Before printing, we compute the maximum width of the rank field,
 *     the size string, and (for dirs) the file-count field, so all
 *     columns are right-aligned for a clean tabular look.
 */

#ifndef _WIN32
#  define _DEFAULT_SOURCE
#endif

#include "largest.h"
#include "fs.h"
#include "utils.h"
#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Entry visibility (mirrors tree.c entry_is_visible)                 */
/* ------------------------------------------------------------------ */

static int lg_entry_visible(const char *name, int is_hidden, int is_dir,
                              const char *rel_prefix,
                              const cli_opts_t *opts) {
    if (ignore_match(name, opts->ignore, opts->ignore_count))
        return 0;
    if (!opts->show_all && is_hidden)
        return 0;
    if (!is_dir && opts->pattern) {
        char rel_path[4096];
        int n = snprintf(rel_path, sizeof(rel_path), "%s%s", rel_prefix, name);
        if (n < 0 || n >= (int)sizeof(rel_path))
            return 0;
        if (!glob_match_path(rel_path, opts->pattern))
            return 0;
    }
    return 1;
}

/* ================================================================== */
/*  LARGEST FILES                                                       */
/* ================================================================== */

#define LF_PATH_MAX 4096

typedef struct {
    char      path[LF_PATH_MAX];
    long long size_bytes;
} lf_entry_t;

typedef struct {
    lf_entry_t *data;
    int         len;
    int         cap;
} lf_vec_t;

static void lf_vec_init(lf_vec_t *v) {
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

static int lf_vec_push(lf_vec_t *v, const char *path, long long sz) {
    if (v->len == v->cap) {
        int         new_cap = v->cap ? v->cap * 2 : 64;
        lf_entry_t *tmp     = (lf_entry_t *)realloc(v->data,
                               (size_t)new_cap * sizeof(lf_entry_t));
        if (!tmp) return 0;
        v->data = tmp;
        v->cap  = new_cap;
    }
    strncpy(v->data[v->len].path, path, LF_PATH_MAX - 1);
    v->data[v->len].path[LF_PATH_MAX - 1] = '\0';
    v->data[v->len].size_bytes = sz;
    v->len++;
    return 1;
}

static void lf_vec_free(lf_vec_t *v) {
    free(v->data);
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

static int lf_cmp_desc(const void *a, const void *b) {
    const lf_entry_t *fa = (const lf_entry_t *)a;
    const lf_entry_t *fb = (const lf_entry_t *)b;
    if (fb->size_bytes != fa->size_bytes)
        return (fb->size_bytes > fa->size_bytes) ? 1 : -1;
    return strcmp(fa->path, fb->path); /* stable: alphabetical on tie */
}

/* lf_walk -- collect all visible files under dir_path into results */
static void lf_walk(const char *dir_path, const char *rel_prefix,
                     const cli_opts_t *opts, int depth,
                     lf_vec_t *results) {
    fs_dir_t  *dir;
    fs_entry_t e;

    if (opts->max_depth != -1 && depth >= opts->max_depth)
        return;

    dir = fs_opendir(dir_path);
    if (!dir) return;

    while (fs_readdir(dir, &e)) {
        int is_dir = (e.type == FS_ENTRY_DIR);

        if (!lg_entry_visible(e.name, e.is_hidden, is_dir, rel_prefix, opts))
            continue;

        if (is_dir) {
            char child_path[4096];
            char child_rel[4096];
            if (!fs_join(child_path, sizeof(child_path), dir_path, e.name))
                continue;
            snprintf(child_rel, sizeof(child_rel), "%s%s/", rel_prefix, e.name);
            lf_walk(child_path, child_rel, opts, depth + 1, results);
        } else {
            char full_path[4096];
            if (!fs_join(full_path, sizeof(full_path), dir_path, e.name))
                continue;

            {
                long long sz = fs_get_size(full_path);
                if (sz < 0) sz = 0;

                /* Build display path = rel_prefix + name */
                char disp[4096];
                snprintf(disp, sizeof(disp), "%s%s", rel_prefix, e.name);

                if (!lf_vec_push(results, disp, sz))
                    fprintf(stderr, "neotree: out of memory\n");
            }
        }
    }
    fs_closedir(dir);
}

/* print_lf -- print top `n` entries from sorted lf_vec_t */
static void print_lf(const lf_entry_t *data, int count, int n) {
    int limit  = (n < count) ? n : count;
    int i;

    printf("Largest Files\n");
    printf("-------------\n\n");
    for (i = 0; i < limit; i++) {
        char sz[32];
        format_size(data[i].size_bytes, sz, sizeof(sz));
        const char *p = data[i].path;
        char truncated[32];
        if (strlen(p) > 24) {
            utf8_truncate(truncated, sizeof(truncated), p, 21);
            p = truncated;
        }
        printf("%d. %-24s  %s\n", i + 1, p, sz);
    }
}

void largest_files_print(const char * const roots[], int roots_count,
                          const cli_opts_t *opts, int n) {
    lf_vec_t results;
    int      r;

    lf_vec_init(&results);

    for (r = 0; r < roots_count; r++) {
        /* Build display prefix: "./" for ".", "root_name/" otherwise */
        char prefix[4096];
        size_t rlen = strlen(roots[r]);
        if (rlen == 1 && roots[r][0] == '.') {
            prefix[0] = '\0'; /* paths shown as-is relative to root */
        } else {
            snprintf(prefix, sizeof(prefix), "%s/", roots[r]);
        }
        lf_walk(roots[r], prefix, opts, 0, &results);
    }

    if (results.len == 0) {
        printf("No files found.\n");
        lf_vec_free(&results);
        return;
    }

    qsort(results.data, (size_t)results.len, sizeof(lf_entry_t), lf_cmp_desc);
    print_lf(results.data, results.len, n);
    lf_vec_free(&results);
}

/* ================================================================== */
/*  LARGEST DIRECTORIES                                                 */
/* ================================================================== */

#define LD_PATH_MAX 4096

typedef struct {
    char      path[LD_PATH_MAX]; /* display path (e.g. "./" or "./src") */
    long long size_bytes;        /* recursive total of visible file bytes */
    int       file_count;        /* recursive count of visible files */
} ld_entry_t;

typedef struct {
    ld_entry_t *data;
    int         len;
    int         cap;
} ld_vec_t;

static void ld_vec_init(ld_vec_t *v) {
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

static int ld_vec_push(ld_vec_t *v, const char *path,
                        long long sz, int fc) {
    if (v->len == v->cap) {
        int         new_cap = v->cap ? v->cap * 2 : 64;
        ld_entry_t *tmp     = (ld_entry_t *)realloc(v->data,
                               (size_t)new_cap * sizeof(ld_entry_t));
        if (!tmp) return 0;
        v->data = tmp;
        v->cap  = new_cap;
    }
    strncpy(v->data[v->len].path, path, LD_PATH_MAX - 1);
    v->data[v->len].path[LD_PATH_MAX - 1] = '\0';
    v->data[v->len].size_bytes = sz;
    v->data[v->len].file_count = fc;
    v->len++;
    return 1;
}

static void ld_vec_free(ld_vec_t *v) {
    free(v->data);
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

static int ld_cmp_desc(const void *a, const void *b) {
    const ld_entry_t *da = (const ld_entry_t *)a;
    const ld_entry_t *db = (const ld_entry_t *)b;
    if (db->size_bytes != da->size_bytes)
        return (db->size_bytes > da->size_bytes) ? 1 : -1;
    return strcmp(da->path, db->path);
}

/*
 * ld_subtree_t -- returned by ld_walk to communicate subtree totals
 * back to the parent for recording.
 */
typedef struct {
    long long bytes;
    int       files;
} ld_sub_t;

/* ld_walk -- recursively collect dir sizes into results */
static ld_sub_t ld_walk(const char *dir_path, const char *disp_path,
                        const char *rel_prefix,
                        const cli_opts_t *opts, int depth,
                        ld_vec_t *results) {
    fs_dir_t  *dir;
    fs_entry_t e;
    ld_sub_t   total;

    total.bytes = 0;
    total.files = 0;

    if (opts->max_depth != -1 && depth >= opts->max_depth) {
        /* At depth limit: add this dir as a leaf with unknown contents */
        if (!ld_vec_push(results, disp_path, 0LL, 0))
            fprintf(stderr, "neotree: out of memory\n");
        return total;
    }

    dir = fs_opendir(dir_path);
    if (!dir) return total;

    while (fs_readdir(dir, &e)) {
        int is_dir = (e.type == FS_ENTRY_DIR);

        if (!lg_entry_visible(e.name, e.is_hidden, is_dir, rel_prefix, opts))
            continue;

        if (is_dir) {
            char child_path[4096];
            char child_disp[4096];
            char child_rel[4096];
            ld_sub_t sub;
            if (!fs_join(child_path, sizeof(child_path), dir_path, e.name))
                continue;

            /* Build display path safely: preserve separator if parent ends with one, else add '/' */
            size_t dlen = strlen(disp_path);
            if (dlen > 0 && (disp_path[dlen - 1] == '/' || disp_path[dlen - 1] == '\\')) {
                snprintf(child_disp, sizeof(child_disp), "%s%s", disp_path, e.name);
            } else {
                snprintf(child_disp, sizeof(child_disp), "%s/%s", disp_path, e.name);
            }

            /* Build relative prefix (always ends with '/') */
            snprintf(child_rel, sizeof(child_rel), "%s%s/", rel_prefix, e.name);

            sub = ld_walk(child_path, child_disp, child_rel, opts, depth + 1, results);
            total.bytes += sub.bytes;
            total.files += sub.files;
        } else {
            char full_path[4096];
            long long sz;
            if (!fs_join(full_path, sizeof(full_path), dir_path, e.name))
                continue;
            sz = fs_get_size(full_path);
            if (sz < 0) sz = 0;
            total.bytes += sz;
            total.files++;
        }
    }
    fs_closedir(dir);

    /* Record this directory */
    if (!ld_vec_push(results, disp_path, total.bytes, total.files))
        fprintf(stderr, "neotree: out of memory\n");

    return total;
}

/* print_ld -- print top `n` directory entries from sorted ld_vec_t */
static void print_ld(const ld_entry_t *data, int count, int n) {
    int limit   = (n < count) ? n : count;
    int i;

    printf("Largest Directories\n");
    printf("-------------------\n\n");
    for (i = 0; i < limit; i++) {
        char sz[32];
        format_size(data[i].size_bytes, sz, sizeof(sz));
        const char *p = data[i].path;
        char truncated[32];
        if (strlen(p) > 24) {
            utf8_truncate(truncated, sizeof(truncated), p, 21);
            p = truncated;
        }
        printf("%d. %-24s  %-8s  (%d file%s)\n",
               i + 1, p, sz,
               data[i].file_count,
               data[i].file_count == 1 ? "" : "s");
    }
}

void largest_dirs_print(const char * const roots[], int roots_count,
                         const cli_opts_t *opts, int n) {
    ld_vec_t results;
    int      r;

    ld_vec_init(&results);

    for (r = 0; r < roots_count; r++) {
        /* Display path for root: "./" or "root_name/" */
        char disp_root[4096];
        char rel_root[4096];
        size_t rlen = strlen(roots[r]);
        int needs_slash = (rlen > 0 &&
                           roots[r][rlen - 1] != '/' &&
                           roots[r][rlen - 1] != '\\');
        snprintf(disp_root, sizeof(disp_root), "%s%s",
                 roots[r], needs_slash ? "/" : "");

        if (rlen == 1 && roots[r][0] == '.') {
            rel_root[0] = '\0';
        } else {
            snprintf(rel_root, sizeof(rel_root), "%s%s",
                     roots[r], needs_slash ? "/" : "");
        }

        ld_walk(roots[r], disp_root, rel_root, opts, 0, &results);
    }

    if (results.len == 0) {
        printf("No directories found.\n");
        ld_vec_free(&results);
        return;
    }

    qsort(results.data, (size_t)results.len, sizeof(ld_entry_t), ld_cmp_desc);
    print_ld(results.data, results.len, n);
    ld_vec_free(&results);
}
