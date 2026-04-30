/*
 * utils.c — miscellaneous helpers implementation
 */

/* strdup is a POSIX extension; request it explicitly on glibc */
#ifndef _WIN32
#  define _DEFAULT_SOURCE
#endif

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Color                                                               */
/* ------------------------------------------------------------------ */

/*
 * isatty detection: POSIX has <unistd.h>, Windows has <io.h>.
 * We wrap it so the rest of the code never sees platform details.
 */
#ifdef _WIN32
#  include <io.h>
#  define IS_TTY(fd)  _isatty(fd)
#  define STDOUT_FD   1
#else
#  include <unistd.h>
#  define IS_TTY(fd)  isatty(fd)
#  define STDOUT_FD   STDOUT_FILENO
#endif

static int g_color = 0;

void color_init(int force_off) {
    if (force_off) {
        g_color = 0;
    } else {
        /* Only emit ANSI codes when stdout is a real terminal.
         * This means piping to a file or pager gets plain text
         * automatically — no need for the caller to pass --no-color. */
        g_color = IS_TTY(STDOUT_FD);
    }
}

int color_enabled(void) { return g_color; }

const char *color_reset(void) {
    return g_color ? "\033[0m" : "";
}

/*
 * color_for_name — picks an ANSI color based on entry type / extension.
 * Extend the extension table here as desired.
 */
const char *color_for_name(const char *name, int is_dir) {
    if (!g_color) return "";

    if (is_dir) return "\033[1;34m";  /* bold blue   */

    const char *ext = strrchr(name, '.');
    if (!ext) return "\033[0m";

    /* C family */
    if (strcmp(ext, ".c")   == 0 ||
        strcmp(ext, ".h")   == 0) return "\033[32m";    /* green       */

    /* C++ */
    if (strcmp(ext, ".cpp") == 0 ||
        strcmp(ext, ".cc")  == 0 ||
        strcmp(ext, ".hpp") == 0) return "\033[32m";

    /* Python */
    if (strcmp(ext, ".py")  == 0) return "\033[33m";    /* yellow      */

    /* Shell */
    if (strcmp(ext, ".sh")  == 0 ||
        strcmp(ext, ".bash")== 0) return "\033[32m";

    /* Web */
    if (strcmp(ext, ".html")== 0 ||
        strcmp(ext, ".css") == 0 ||
        strcmp(ext, ".js")  == 0 ||
        strcmp(ext, ".ts")  == 0) return "\033[35m";    /* magenta     */

    /* Config / data */
    if (strcmp(ext, ".json")== 0 ||
        strcmp(ext, ".toml")== 0 ||
        strcmp(ext, ".yaml")== 0 ||
        strcmp(ext, ".yml") == 0) return "\033[36m";    /* cyan        */

    /* Rust */
    if (strcmp(ext, ".rs")  == 0) return "\033[31m";    /* red         */

    /* Markdown / text */
    if (strcmp(ext, ".md")  == 0 ||
        strcmp(ext, ".txt") == 0) return "\033[0m";

    return "\033[0m";
}

/* ------------------------------------------------------------------ */
/*  Matching                                                            */
/* ------------------------------------------------------------------ */

int ignore_match(const char *name,
                 const char * const ignore[], int ignore_count) {
    for (int i = 0; i < ignore_count; i++) {
        if (strcmp(name, ignore[i]) == 0)
            return 1;
    }
    return 0;
}

int pattern_match(const char *name, const char *pattern) {
    if (!pattern) return 1;

    /* *.ext  — suffix match */
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char *ext = strrchr(name, '.');
        return (ext != NULL && strcmp(ext, pattern + 1) == 0);
    }

    /* exact match */
    return strcmp(name, pattern) == 0;
}

/* ------------------------------------------------------------------ */
/*  Dynamic entry array                                                 */
/* ------------------------------------------------------------------ */

#define ENTRY_VEC_INIT_CAP 32

void entry_vec_init(entry_vec_t *v) {
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

int entry_vec_push(entry_vec_t *v, const char *name, int is_dir) {
    if (v->len == v->cap) {
        int new_cap = v->cap ? v->cap * 2 : ENTRY_VEC_INIT_CAP;
        entry_t *tmp = realloc(v->data, (size_t)new_cap * sizeof(entry_t));
        if (!tmp) return 0;   /* OOM — caller checks */
        v->data = tmp;
        v->cap  = new_cap;
    }

    v->data[v->len].name   = strdup(name);
    v->data[v->len].is_dir = is_dir;

    if (!v->data[v->len].name) return 0;  /* strdup OOM */

    v->len++;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Sorting                                                             */
/*                                                                      */
/*  dirs_first = 1  →  dirs < files, alphabetical within each group   */
/*  dirs_first = 0  →  pure alphabetical across all entries           */
/* ------------------------------------------------------------------ */

static int cmp_alpha(const void *a, const void *b) {
    const entry_t *ea = (const entry_t *)a;
    const entry_t *eb = (const entry_t *)b;
    return strcmp(ea->name, eb->name);
}

static int cmp_dirs_first(const void *a, const void *b) {
    const entry_t *ea = (const entry_t *)a;
    const entry_t *eb = (const entry_t *)b;

    if (ea->is_dir != eb->is_dir)
        return eb->is_dir - ea->is_dir;   /* dirs sort lower (earlier) */

    return strcmp(ea->name, eb->name);
}

void entry_vec_sort(entry_vec_t *v, int dirs_first) {
    if (v->len < 2) return;
    qsort(v->data, (size_t)v->len, sizeof(entry_t),
          dirs_first ? cmp_dirs_first : cmp_alpha);
}

void entry_vec_free(entry_vec_t *v) {
    for (int i = 0; i < v->len; i++)
        free(v->data[i].name);
    free(v->data);
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}
