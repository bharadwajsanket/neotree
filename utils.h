#ifndef UTILS_H
#define UTILS_H

/*
 * utils.h — miscellaneous helpers
 *
 * Covers:
 *   - color / ANSI escape management
 *   - entry name matching (ignore list, file pattern)
 *   - simple dynamic array for collecting dir entries
 */

#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Color                                                               */
/* ------------------------------------------------------------------ */

/*
 * color_init — call once at startup.
 *   force_off = 1  → always disable color (--no-color).
 *   force_off = 0  → enable color only when stdout is a TTY.
 */
void color_init(int force_off);

/*
 * color_enabled — returns 1 if ANSI output is active.
 */
int color_enabled(void);

/* ANSI codes — empty strings when color is disabled */
const char *color_reset(void);
const char *color_for_name(const char *name, int is_dir);

/* ------------------------------------------------------------------ */
/*  Matching                                                            */
/* ------------------------------------------------------------------ */

/*
 * ignore_match — returns 1 if `name` is in the ignore list.
 *                Simple exact-match for now; extend here if needed.
 */
int ignore_match(const char *name,
                 const char * const ignore[], int ignore_count);

/*
 * pattern_match — returns 1 if `name` matches `pattern`.
 *   pattern NULL  → always match.
 *   pattern "*.c" → suffix match.
 *   otherwise     → exact match.
 */
int pattern_match(const char *name, const char *pattern);

/* ------------------------------------------------------------------ */
/*  Dynamic entry array (used by tree.c to collect + sort entries)     */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *name;   /* heap-allocated copy */
    int    is_dir;
} entry_t;

typedef struct {
    entry_t *data;
    int      len;
    int      cap;
} entry_vec_t;

void  entry_vec_init(entry_vec_t *v);
int   entry_vec_push(entry_vec_t *v, const char *name, int is_dir);
void  entry_vec_sort(entry_vec_t *v, int dirs_first);
void  entry_vec_free(entry_vec_t *v);

#endif /* UTILS_H */
