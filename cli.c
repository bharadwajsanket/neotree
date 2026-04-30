/*
 * cli.c — command-line argument parsing implementation
 *
 * Follows standard UNIX conventions:
 *   - short flags: -L <n>
 *   - long flags:  --ignore <pattern>, --no-color, --dirs-first, etc.
 *   - bare argument: treated as the root path
 *   - --  can be used to end flag parsing
 *
 * Defaults:
 *   root       = "."
 *   max_depth  = -1  (unlimited)
 *   pattern    = NULL (all files)
 *   no_color   = 0  (color enabled, subject to isatty check in utils.c)
 *   dirs_first = 1
 *   ignore     = { ".git", "node_modules", "__pycache__", "target", "build" }
 */

#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Default ignore list                                                 */
/* ------------------------------------------------------------------ */

static const char *DEFAULT_IGNORE[] = {
    ".git",
    "node_modules",
    "__pycache__",
    "target",
    "build",
};
#define DEFAULT_IGNORE_COUNT  (int)(sizeof(DEFAULT_IGNORE) / sizeof(*DEFAULT_IGNORE))

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */

static void die_usage(const char *msg, const char *arg) {
    if (arg)
        fprintf(stderr, "neotree: %s: '%s'\n", msg, arg);
    else
        fprintf(stderr, "neotree: %s\n", msg);
    fprintf(stderr, "Try 'neotree --help' for usage.\n");
    exit(1);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void cli_usage(void) {
    printf(
        "Usage: neotree [OPTIONS] [PATH]\n"
        "\n"
        "Display a directory tree.\n"
        "\n"
        "Arguments:\n"
        "  PATH                  Root directory to display (default: .)\n"
        "\n"
        "Options:\n"
        "  -L <depth>            Limit display depth (1 = root entries only)\n"
        "  --ignore <name>       Ignore entries matching name (repeatable)\n"
        "  --pattern <glob>      Show only files matching pattern (e.g. *.c)\n"
        "  --no-color            Disable ANSI color output\n"
        "  --no-dirs-first       List files and dirs in mixed order\n"
        "  -h, --help            Show this help and exit\n"
        "  --version             Show version and exit\n"
        "\n"
        "Default ignored dirs: .git, node_modules, __pycache__, target, build\n"
        "\n"
        "Examples:\n"
        "  neotree\n"
        "  neotree src/\n"
        "  neotree -L 2 .\n"
        "  neotree --pattern '*.c' src/\n"
        "  neotree --ignore dist --ignore .cache .\n"
        "  neotree --no-color | tee tree.txt\n"
    );
    exit(0);
}

void cli_parse(int argc, char *argv[], cli_opts_t *opts) {
    /* --- defaults --- */
    opts->root        = ".";
    opts->max_depth   = -1;
    opts->pattern     = NULL;
    opts->no_color    = 0;
    opts->dirs_first  = 1;
    opts->ignore_count = 0;

    /* seed with defaults */
    for (int i = 0; i < DEFAULT_IGNORE_COUNT; i++) {
        if (opts->ignore_count < CLI_MAX_IGNORE)
            opts->ignore[opts->ignore_count++] = DEFAULT_IGNORE[i];
    }

    int end_of_flags = 0; /* set after -- */

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (end_of_flags || arg[0] != '-') {
            /* treat as root path */
            opts->root = arg;
            continue;
        }

        /* -- signals end of flags */
        if (strcmp(arg, "--") == 0) {
            end_of_flags = 1;
            continue;
        }

        /* --help / -h */
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            cli_usage();
        }

        /* --version */
        if (strcmp(arg, "--version") == 0) {
            printf("%s %s\n", CLI_PROGRAM, CLI_VERSION);
            exit(0);
        }

        /* -L <depth> */
        if (strcmp(arg, "-L") == 0) {
            if (i + 1 >= argc) die_usage("-L requires a depth argument", NULL);
            char *end;
            long v = strtol(argv[++i], &end, 10);
            if (*end != '\0' || v < 1)
                die_usage("-L depth must be a positive integer", argv[i]);
            opts->max_depth = (int)v;
            continue;
        }

        /* --ignore <name> */
        if (strcmp(arg, "--ignore") == 0) {
            if (i + 1 >= argc) die_usage("--ignore requires an argument", NULL);
            if (opts->ignore_count >= CLI_MAX_IGNORE)
                die_usage("too many --ignore entries (max " 
                          /* stringified below */"64)", NULL);
            opts->ignore[opts->ignore_count++] = argv[++i];
            continue;
        }

        /* --pattern <glob> */
        if (strcmp(arg, "--pattern") == 0) {
            if (i + 1 >= argc) die_usage("--pattern requires an argument", NULL);
            opts->pattern = argv[++i];
            continue;
        }

        /* --no-color */
        if (strcmp(arg, "--no-color") == 0) {
            opts->no_color = 1;
            continue;
        }

        /* --no-dirs-first */
        if (strcmp(arg, "--no-dirs-first") == 0) {
            opts->dirs_first = 0;
            continue;
        }

        /* unknown flag */
        die_usage("unknown option", arg);
    }
}
