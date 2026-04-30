#ifndef CLI_H
#define CLI_H

/*
 * cli.h — command-line argument parsing
 *
 * Owns all argc/argv logic. Produces a single `cli_opts_t` struct
 * that the rest of the program reads from. Nothing else touches argv.
 */

#define CLI_MAX_IGNORE  64   /* max --ignore entries          */
#define CLI_VERSION     "0.1.0"
#define CLI_PROGRAM     "neotree"

/* ------------------------------------------------------------------ */

typedef struct {
    const char *root;           /* path to display (default ".")       */
    int         max_depth;      /* -L value; -1 = unlimited            */
    const char *pattern;        /* glob/name pattern; NULL = all       */
    int         no_color;       /* 1 = disable ANSI color              */
    int         dirs_first;     /* 1 = dirs before files (default: 1)  */

    const char *ignore[CLI_MAX_IGNORE];
    int         ignore_count;
} cli_opts_t;

/*
 * cli_parse — parse argc/argv into opts.
 *   - Prints usage and exits on bad input.
 *   - Prints version and exits on --version.
 *   - Prints help  and exits on --help / -h.
 */
void cli_parse(int argc, char *argv[], cli_opts_t *opts);

/*
 * cli_usage — print help text to stdout and exit(0).
 */
void cli_usage(void);

#endif /* CLI_H */
