#ifndef CLI_H
#define CLI_H

/*
 * cli.h — command-line argument parsing
 *
 * Owns all argc/argv logic.  Produces a single `cli_opts_t` struct
 * that the rest of the program reads from.  Nothing else touches argv.
 */

#define CLI_MAX_IGNORE   64    /* max --ignore entries (CLI + .gitignore) */
#define CLI_VERSION      "0.2.4"
#define CLI_PROGRAM      "neotree"

/* ------------------------------------------------------------------ */

typedef struct {
    const char *root;           /* path to display (default ".")       */
    int         max_depth;      /* -L value; -1 = unlimited            */
    const char *pattern;        /* glob/name pattern; NULL = all       */
    int         no_color;       /* 1 = disable ANSI color              */
    int         dirs_first;     /* 1 = dirs before files (default: 1)  */
    int         show_all;       /* 1 = show hidden (dot) entries       */
    int         dirs_only;      /* 1 = print directories only          */
    int         show_size;      /* 1 = print file size in KB           */

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
 *   - Reads ROOT/.gitignore and merges patterns into the ignore list.
 */
void cli_parse(int argc, char *argv[], cli_opts_t *opts);

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
