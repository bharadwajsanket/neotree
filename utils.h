#ifndef UTILS_H
#define UTILS_H

/*
 * utils.h -- miscellaneous helpers
 *
 * Covers:
 *   - color / ANSI escape management
 *   - entry name matching (ignore list, simple glob, path-aware glob)
 *   - simple dynamic array for collecting dir entries
 *   - extension summary table (for --ext-summary)
 */

#include <stddef.h>
#include "cli.h"   /* sort_by_t */

/* ------------------------------------------------------------------ */
/*  Color                                                               */
/* ------------------------------------------------------------------ */

/*
 * color_init -- call once at startup.
 *   force_off = 1 -> always disable color (--no-color).
 *   force_off = 0 -> enable color only when stdout is a TTY.
 */
void color_init(int force_off);

/* Returns 1 if ANSI output is currently active. */
int color_enabled(void);

/* ANSI escape sequences -- all return "" when color is disabled. */
const char *color_reset(void);
const char *color_for_name(const char *name, int is_dir);
const char *color_for_exec(void);   /* red -- for executable files */

/* ------------------------------------------------------------------ */
/*  Matching                                                            */
/* ------------------------------------------------------------------ */

/*
 * ignore_match -- returns 1 if name matches any ignore-list entry.
 *
 * Each entry is checked as:
 *   1. Exact string match  (e.g. "build", ".cache")
 *   2. Extension glob      (e.g. "*.log", "*.o")
 */
int ignore_match(const char *name,
                 const char * const ignore[], int ignore_count);

/*
 * pattern_match -- returns 1 if name matches pattern.
 *
 * Simple (non-path) matcher used for ignore-list entries and plain
 * --pattern values that do NOT contain "**".
 *
 *   NULL      -> always match
 *   "*"       -> match anything
 *   "*.ext"   -> suffix match on the file extension
 *   "literal" -> exact string match
 *
 * For patterns containing "**", use glob_match_path() instead.
 */
int pattern_match(const char *name, const char *pattern);

/*
 * pattern_has_doublestar -- returns 1 if pattern contains "**".
 * When true, tree.c will always recurse into directories and use
 * glob_match_path() to test files against the full relative path.
 */
int pattern_has_doublestar(const char *pattern);

/*
 * glob_match_path -- path-aware recursive glob matcher for ** patterns.
 *
 * Arguments:
 *   rel_path  relative path from traversal root, '/' separated.
 *             Examples: "src/internal/foo.h", "main.c"
 *   pattern   glob pattern using '/' as separator.
 *             Examples: see table below.
 *
 * Pattern rules:
 *   *      matches any sequence of non-'/' characters
 *   **     matches zero or more complete path components
 *   /      matched literally
 *   other  matched literally
 *
 * Match table (STARSTAR represents the literal string **):
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
 * (Actual patterns use ** not STARSTAR; table avoids the end-comment
 *  sequence for C preprocessor compatibility.)
 *
 * Limitations:
 *   - No character classes ([abc]) or ? wildcards.
 *   - Pattern separators must be '/' (even on Windows).
 *   - Not full bash glob emulation.
 */
int glob_match_path(const char *rel_path, const char *pattern);

/*
 * is_hidden -- returns 1 if name starts with '.'.
 */
int is_hidden(const char *name);

/* ------------------------------------------------------------------ */
/*  Dynamic entry array                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *name;     /* heap-allocated copy */
    int    is_dir;
    long   size_kb;  /* file size in KB; -1 if unknown or directory */
    long   mtime;    /* modification time (seconds since epoch); 0 if unknown */
} entry_t;

typedef struct {
    entry_t *data;
    int      len;
    int      cap;
} entry_vec_t;

void  entry_vec_init(entry_vec_t *v);
int   entry_vec_push(entry_vec_t *v, const char *name, int is_dir,
                     long size_kb, long mtime);
void  entry_vec_sort(entry_vec_t *v, int dirs_first, sort_by_t sort_by);
void  entry_vec_free(entry_vec_t *v);

/* ------------------------------------------------------------------ */
/*  Extension summary table                                             */
/* ------------------------------------------------------------------ */

#define EXT_TABLE_MAX 128

typedef struct {
    char ext[32];
    int  count;
} ext_entry_t;

typedef struct {
    ext_entry_t entries[EXT_TABLE_MAX];
    int         len;
} ext_table_t;

void ext_table_init(ext_table_t *t);
void ext_table_add(ext_table_t *t, const char *filename);
void ext_table_print(const ext_table_t *t);

#endif /* UTILS_H */
