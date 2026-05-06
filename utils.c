/*
 * utils.c -- miscellaneous helpers implementation
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
         * Piping to a file or pager automatically gets plain text. */
        g_color = IS_TTY(STDOUT_FD);
    }
}

int color_enabled(void) { return g_color; }

const char *color_reset(void) {
    return g_color ? "\033[0m" : "";
}

const char *color_for_exec(void) {
    return g_color ? "\033[0;31m" : "";   /* red -- executables */
}

/*
 * color_for_name -- picks an ANSI color based on entry type / extension.
 *
 * Colors (per spec):
 *   directories              -- bold blue  (unchanged)
 *   .c, .h, .cpp, .hpp, ... -- cyan
 *   .py                      -- yellow
 *   .md, .txt, .rst          -- green
 *   .sh, .bash, .zsh, .fish  -- magenta
 *   images (.png/.jpg/...)   -- purple (bright magenta)
 *   executables              -- red (caller uses color_for_exec())
 *   unknown / default        -- normal terminal color (reset)
 */
const char *color_for_name(const char *name, int is_dir) {
    if (!g_color) return "";

    if (is_dir) return "\033[1;34m";  /* bold blue */

    const char *ext = strrchr(name, '.');
    if (!ext) return "\033[0m";

    /* C / C++ */
    if (strcmp(ext, ".c")    == 0 ||
        strcmp(ext, ".h")    == 0 ||
        strcmp(ext, ".cpp")  == 0 ||
        strcmp(ext, ".cc")   == 0 ||
        strcmp(ext, ".cxx")  == 0 ||
        strcmp(ext, ".hpp")  == 0) return "\033[0;36m";   /* cyan */

    /* Python */
    if (strcmp(ext, ".py")   == 0) return "\033[0;33m";   /* yellow */

    /* Markdown / plain text */
    if (strcmp(ext, ".md")   == 0 ||
        strcmp(ext, ".txt")  == 0 ||
        strcmp(ext, ".rst")  == 0) return "\033[0;32m";   /* green */

    /* Shell scripts */
    if (strcmp(ext, ".sh")   == 0 ||
        strcmp(ext, ".bash") == 0 ||
        strcmp(ext, ".zsh")  == 0 ||
        strcmp(ext, ".fish") == 0) return "\033[0;35m";   /* magenta */

    /* Images -- purple (bright magenta) */
    if (strcmp(ext, ".png")  == 0 ||
        strcmp(ext, ".jpg")  == 0 ||
        strcmp(ext, ".jpeg") == 0 ||
        strcmp(ext, ".gif")  == 0 ||
        strcmp(ext, ".webp") == 0 ||
        strcmp(ext, ".svg")  == 0 ||
        strcmp(ext, ".bmp")  == 0) return "\033[0;95m";

    /* Rust */
    if (strcmp(ext, ".rs")   == 0) return "\033[0;31m";   /* red */

    /* Web */
    if (strcmp(ext, ".html") == 0 ||
        strcmp(ext, ".css")  == 0 ||
        strcmp(ext, ".js")   == 0 ||
        strcmp(ext, ".ts")   == 0) return "\033[0;35m";   /* magenta */

    /* Config / data */
    if (strcmp(ext, ".json") == 0 ||
        strcmp(ext, ".toml") == 0 ||
        strcmp(ext, ".yaml") == 0 ||
        strcmp(ext, ".yml")  == 0) return "\033[0;36m";   /* cyan */

    return "\033[0m";
}

/* ------------------------------------------------------------------ */
/*  Matching                                                            */
/* ------------------------------------------------------------------ */

/*
 * ignore_match -- returns 1 if name matches any entry in the ignore list.
 *
 * Each entry is tested as:
 *   1. Exact match  (e.g. "build", ".cache")
 *   2. Suffix glob  (e.g. "*.log", "*.o")
 */
int ignore_match(const char *name,
                 const char * const ignore[], int ignore_count) {
    for (int i = 0; i < ignore_count; i++) {
        if (strcmp(name, ignore[i]) == 0)
            return 1;
        if (ignore[i][0] == '*' && pattern_match(name, ignore[i]))
            return 1;
    }
    return 0;
}

/*
 * pattern_match -- returns 1 if name matches pattern.
 *
 * Used for simple (non-**) patterns and for the ignore list.
 *
 *   NULL      -> always match
 *   "*"       -> match anything
 *   "*.ext"   -> suffix match on the extension
 *   "literal" -> exact string match
 *
 * ** patterns must be handled by glob_match_path() instead.
 */
int pattern_match(const char *name, const char *pattern) {
    if (!pattern) return 1;

    /* Bare "*" -- match anything */
    if (strcmp(pattern, "*") == 0)
        return 1;

    /* "*.ext" -- extension suffix match */
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char *ext = strrchr(name, '.');
        return (ext != NULL && strcmp(ext, pattern + 1) == 0);
    }

    /* exact match */
    return strcmp(name, pattern) == 0;
}

/*
 * pattern_has_doublestar -- returns 1 if pattern contains "**".
 * Used by tree.c to decide whether directories must always be descended
 * (so that files at any depth can be reached and matched).
 */
int pattern_has_doublestar(const char *pattern) {
    if (!pattern) return 0;
    return (strstr(pattern, "**") != NULL);
}

/* ------------------------------------------------------------------ */
/*  Path-aware recursive glob matching                                  */
/* ------------------------------------------------------------------ */

/*
 * glob_match_path -- match a relative file path against a glob pattern.
 *
 * Called from tree.c for ** patterns.  Operates on the FULL relative
 * path from the traversal root (e.g. "src/internal/foo.h"), not just
 * the filename basename.
 *
 * Pattern rules:
 *   *      matches any sequence of non-'/' characters
 *   **     matches zero or more complete path components
 *          (i.e. crosses directory boundaries)
 *   /      matched literally as path separator
 *   other  matched literally, character by character
 *
 * Concrete examples:
 * Match table (STARSTAR = literal ** double-wildcard):
 *   pattern              rel_path            result
 *   STARSTAR/.h          cli.h               MATCH
 *   STARSTAR/.h          src/foo.h           MATCH
 *   src/STARSTAR/.h      src/foo.h           MATCH
 *   src/STARSTAR/.h      src/a/b/c.h         MATCH
 *   src/STARSTAR/.h      cli.h               NO MATCH
 *   src/STARSTAR/.h      include/test.h      NO MATCH
 *   STARSTAR/.c          main.c              MATCH
 *   STARSTAR/.c          src/tree.c          MATCH
 *
 * (Patterns use ** not STARSTAR; table avoids C end-comment sequence.)
 *
 * Limitations:
 *   - No character classes ([abc]) or ? wildcards.
 *   - Path separators in patterns must be '/' (even on Windows).
 *   - Not full bash glob emulation.
 */

/* Forward declaration for mutual recursion */
static int gmatch(const char *path, const char *pat);

/*
 * gmatch_doublestar -- called after "**" was consumed from pat.
 * Tries matching pat against the current position and every position
 * after a '/' (so ** matches zero or more components).
 */
static int gmatch_doublestar(const char *path, const char *pat) {
    for (;;) {
        if (gmatch(path, pat)) return 1;
        path = strchr(path, '/');
        if (!path) return 0;
        path++;  /* skip the '/' and try from the next component */
    }
}

static int gmatch(const char *path, const char *pat) {
    while (*pat) {
        if (pat[0] == '*' && pat[1] == '*') {
            pat += 2;
            if (*pat == '/') pat++;  /* consume the separator after ** */
            return gmatch_doublestar(path, pat);
        } else if (*pat == '*') {
            pat++;
            /* Match any run of non-'/' chars */
            for (;;) {
                if (gmatch(path, pat)) return 1;
                if (*path == '/' || *path == '\0') return 0;
                path++;
            }
        } else {
            if (*path != *pat) return 0;
            path++;
            pat++;
        }
    }
    return (*path == '\0');
}

int glob_match_path(const char *rel_path, const char *pattern) {
    if (!pattern) return 1;        /* NULL pattern -> match everything */
    if (!rel_path || *rel_path == '\0') return 0;
    return gmatch(rel_path, pattern);
}

/* ------------------------------------------------------------------ */
/*  is_hidden                                                           */
/* ------------------------------------------------------------------ */

/*
 * is_hidden -- returns 1 if the entry name starts with '.'.
 * Used by tree.c to filter hidden files when --all is not set.
 */
int is_hidden(const char *name) {
    return (name[0] == '.');
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

int entry_vec_push(entry_vec_t *v, const char *name, int is_dir,
                   long size_kb, long mtime) {
    if (v->len == v->cap) {
        int new_cap = v->cap ? v->cap * 2 : ENTRY_VEC_INIT_CAP;
        entry_t *tmp = realloc(v->data, (size_t)new_cap * sizeof(entry_t));
        if (!tmp) return 0;
        v->data = tmp;
        v->cap  = new_cap;
    }

    v->data[v->len].name    = strdup(name);
    v->data[v->len].is_dir  = is_dir;
    v->data[v->len].size_kb = size_kb;
    v->data[v->len].mtime   = mtime;

    if (!v->data[v->len].name) return 0;

    v->len++;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Sorting                                                             */
/*                                                                      */
/*  dirs_first = 1 -> dirs first, secondary key within each group      */
/*  dirs_first = 0 -> secondary key across all entries                 */
/*                                                                      */
/*  SORT_NAME     -> alphabetical (strcmp)                              */
/*  SORT_SIZE     -> ascending size (smaller first)                     */
/*  SORT_MODIFIED -> descending mtime (most recently modified first)    */
/* ------------------------------------------------------------------ */

static sort_by_t g_sort_by = SORT_NAME;

static int cmp_secondary(const entry_t *ea, const entry_t *eb) {
    switch (g_sort_by) {
    case SORT_SIZE:
        if (ea->size_kb != eb->size_kb)
            return (ea->size_kb < eb->size_kb) ? -1 : 1;
        return strcmp(ea->name, eb->name);

    case SORT_MODIFIED:
        if (ea->mtime != eb->mtime)
            return (ea->mtime > eb->mtime) ? -1 : 1;
        return strcmp(ea->name, eb->name);

    case SORT_NAME:
    default:
        return strcmp(ea->name, eb->name);
    }
}

static int cmp_flat(const void *a, const void *b) {
    return cmp_secondary((const entry_t *)a, (const entry_t *)b);
}

static int cmp_dirs_first(const void *a, const void *b) {
    const entry_t *ea = (const entry_t *)a;
    const entry_t *eb = (const entry_t *)b;

    if (ea->is_dir != eb->is_dir)
        return eb->is_dir - ea->is_dir;  /* dirs sort earlier */

    return cmp_secondary(ea, eb);
}

void entry_vec_sort(entry_vec_t *v, int dirs_first, sort_by_t sort_by) {
    if (v->len < 2) return;
    g_sort_by = sort_by;
    qsort(v->data, (size_t)v->len, sizeof(entry_t),
          dirs_first ? cmp_dirs_first : cmp_flat);
}

void entry_vec_free(entry_vec_t *v) {
    for (int i = 0; i < v->len; i++)
        free(v->data[i].name);
    free(v->data);
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

/* ------------------------------------------------------------------ */
/*  Extension summary table                                             */
/* ------------------------------------------------------------------ */

void ext_table_init(ext_table_t *t) {
    t->len = 0;
}

/*
 * ext_table_add -- record the extension of filename.
 * Files with no extension are recorded under "" (empty key).
 */
void ext_table_add(ext_table_t *t, const char *filename) {
    const char *ext = strrchr(filename, '.');
    const char *key = ext ? ext : "";

    for (int i = 0; i < t->len; i++) {
        if (strcmp(t->entries[i].ext, key) == 0) {
            t->entries[i].count++;
            return;
        }
    }

    if (t->len >= EXT_TABLE_MAX) return;   /* table full */

    strncpy(t->entries[t->len].ext, key, sizeof(t->entries[0].ext) - 1);
    t->entries[t->len].ext[sizeof(t->entries[0].ext) - 1] = '\0';
    t->entries[t->len].count = 1;
    t->len++;
}

static int ext_sort_cmp(const void *a, const void *b) {
    const ext_entry_t *ea = (const ext_entry_t *)a;
    const ext_entry_t *eb = (const ext_entry_t *)b;
    if (ea->count != eb->count)
        return eb->count - ea->count;  /* descending count */
    return strcmp(ea->ext, eb->ext);   /* ascending alpha tie-breaker */
}

/*
 * ext_table_print -- sort and print the extension summary to stdout.
 *
 * Output format (highest count first, alphabetical tie-breaker):
 *   .c            5
 *   .h            4
 *   (no ext)      1
 */
void ext_table_print(const ext_table_t *t) {
    if (t->len == 0) return;

    ext_entry_t sorted[EXT_TABLE_MAX];
    int n = t->len;
    for (int i = 0; i < n; i++) sorted[i] = t->entries[i];
    qsort(sorted, (size_t)n, sizeof(ext_entry_t), ext_sort_cmp);

    printf("\nExtension summary:\n");
    for (int i = 0; i < n; i++) {
        const char *label = sorted[i].ext[0] ? sorted[i].ext : "(no ext)";
        printf("  %-12s %d\n", label, sorted[i].count);
    }
}
