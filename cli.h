#ifndef CLI_H
#define CLI_H

/*
 * cli.h — command-line argument parsing
 *
 * Owns all argc/argv logic.  Produces a single `cli_opts_t` struct
 * that the rest of the program reads from.  Nothing else touches argv.
 */

#define CLI_MAX_IGNORE   64    /* max --ignore entries (CLI + .gitignore) */
#define CLI_MAX_ROOTS    64    /* max root paths allowed */
#define CLI_VERSION      "0.4.0"
#define CLI_PROGRAM      "neotree"

/* ------------------------------------------------------------------ */

/*
 * Sort keys for --sort.
 * SORT_NAME is the default (alphabetical within group).
 */
typedef enum {
    SORT_NAME     = 0,   /* alphabetical (default)          */
    SORT_SIZE     = 1,   /* ascending file size             */
    SORT_MODIFIED = 2    /* most-recently-modified first    */
} sort_by_t;

typedef struct {
    const char *roots[CLI_MAX_ROOTS]; /* paths to display (default ".") */
    int         roots_count;
    int         max_depth;      /* -L value; -1 = unlimited            */
    const char *pattern;        /* glob/name pattern; NULL = all       */
    int         no_color;       /* 1 = disable ANSI color              */
    int         dirs_first;     /* 1 = dirs before files (default: 1)  */
    int         show_all;       /* 1 = show hidden (dot) entries       */
    int         dirs_only;      /* 1 = print directories only          */
    int         show_size;      /* 1 = print file size in KB           */
    sort_by_t   sort_by;        /* sort key within each group          */
    int         show_stats;     /* 1 = print traversal statistics      */
    int         reverse;        /* 1 = reverse active sort             */
    int         sort_explicit;  /* 1 = --sort was explicitly provided  */
    const char *find;           /* query for file search               */
    const char *find_dir;       /* query for directory search          */
    const char *export_txt;     /* path for plain-text export; NULL=no */
    const char *export_md;      /* path for markdown export;  NULL=no  */

    const char *ignore[CLI_MAX_IGNORE];
    int         ignore_count;

    /*
     * gitignore_start — index into ignore[] where heap-allocated
     * .gitignore patterns begin.  Entries below this index are either
     * static string literals (DEFAULT_IGNORE) or argv pointers, and
     * must NOT be freed.  cli_opts_free() frees [gitignore_start, ignore_count).
     */
    int         gitignore_start;
} cli_opts_t;

/*
 * cli_parse — parse argc/argv into opts.
 *   - Prints usage and exits on bad input.
 *   - Prints version and exits on --version.
 *   - Prints help  and exits on --help / -h.
 */
void cli_parse(int argc, char *argv[], cli_opts_t *opts);

/*
 * cli_load_gitignore — load .gitignore rules for a specific root path.
 */
void cli_load_gitignore(cli_opts_t *opts, const char *root_path);

/*
 * cli_gitignore_free — free only heap-allocated gitignore entries and
 *   reset the ignore_count back to gitignore_start.
 */
void cli_gitignore_free(cli_opts_t *opts);

/*
 * cli_opts_free — release heap memory owned by opts.
 *   Must be called before opts goes out of scope (or before exit in main).
 *   Safe to call on a zero-initialised or already-freed opts.
 */
void cli_opts_free(cli_opts_t *opts);

/*
 * cli_usage — print help text to stdout and exit(0).
 */
void cli_usage(void);

#endif /* CLI_H */
