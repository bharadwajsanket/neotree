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
 * utils_basename -- returns pointer to filename component of a path.
 *
 * Does not allocate.  Returns a pointer into `path` itself.
 * Works on both POSIX ("/a/b/c.txt" -> "c.txt") and Windows.
 */
const char *utils_basename(const char *path);

/* ------------------------------------------------------------------ */
/*  Dynamic entry array                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *name;     /* heap-allocated copy */
    int    is_dir;
    long long size_bytes; /* file size in bytes; -1 if unknown or directory */
    long long mtime;    /* modification time (seconds since epoch); 0 if unknown */
} entry_t;

typedef struct {
    entry_t *data;
    int      len;
    int      cap;
} entry_vec_t;

void  entry_vec_init(entry_vec_t *v);
int   entry_vec_push(entry_vec_t *v, const char *name, int is_dir,
                     long long size_bytes, long long mtime);
void  entry_vec_sort(entry_vec_t *v, int dirs_first, sort_by_t sort_by, int reverse);
void  entry_vec_free(entry_vec_t *v);

void  format_size(long long bytes, char *buf, size_t buf_size);

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

/* ------------------------------------------------------------------ */
/*  Terminal width                                                      */
/* ------------------------------------------------------------------ */

/*
 * term_width_init -- call once at startup (after color_init).
 *   Detects the current terminal column width.
 *   When stdout is not a TTY, stores 0 ("no truncation").
 */
void term_width_init(void);

/*
 * get_terminal_width -- returns detected column width.
 *   0 means "not a TTY / unknown" -- callers must not truncate.
 */
int get_terminal_width(void);

/*
 * utf8_truncate -- truncate src to at most max_cols display columns.
 *   Writes result into dst (must be >= dst_size bytes).
 *   Appends U+2026 (HORIZONTAL ELLIPSIS, 3 UTF-8 bytes) when truncated.
 *   Returns 1 if truncated, 0 if src fit without truncation.
 *
 *   Simplified rule: every UTF-8 code point counts as 1 display column
 *   (correct for Latin, incorrect for wide CJK -- acceptable for a
 *   zero-dependency tool; accurate for the overwhelming majority of
 *   real-world filenames).
 */
int utf8_truncate(char *dst, size_t dst_size, const char *src, int max_cols);

#endif /* UTILS_H */
