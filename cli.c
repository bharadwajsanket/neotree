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
 *   root            = "."
 *   max_depth       = -1  (unlimited)
 *   pattern         = NULL (all files)
 *   no_color        = 0  (color enabled, subject to isatty check in utils.c)
 *   dirs_first      = 1
 *   show_all        = 0  (hide dot-files)
 *   dirs_only       = 0  (show files and dirs)
 *   show_size       = 0  (no size annotation)
 *   sort_by         = SORT_NAME (alphabetical)
 *   ext_summary     = 0  (no summary)
 *   export_txt      = NULL (no export)
 *   export_md       = NULL (no export)
 *   ignore          = { ".git", "node_modules", "__pycache__", "target", "build" }
 *   gitignore_start = ignore_count after defaults + CLI --ignore args
 *
 * .gitignore support:
 *   If a .gitignore file exists in the root directory, its non-empty,
 *   non-comment lines are merged into the ignore list automatically.
 *   This happens in cli_load_gitignore(), called from cli_parse() after
 *   all argv has been processed.
 *
 *   Each pattern loaded from .gitignore is heap-allocated.  The field
 *   gitignore_start marks where these entries begin in opts->ignore[],
 *   allowing cli_opts_free() to free exactly those entries and nothing
 *   else (DEFAULT_IGNORE entries are string literals; --ignore entries
 *   are argv pointers — neither must be freed).
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
/*  .gitignore loader                                                   */
/* ------------------------------------------------------------------ */

/*
 * cli_load_gitignore — read ROOT/.gitignore and append new patterns to
 * opts->ignore[].
 *
 * Rules applied (deliberately minimal — not full gitignore spec):
 *   - Lines longer than the read buffer are discarded entirely (a
 *     truncated pattern would silently match the wrong entries).
 *   - Empty lines and comment lines (first non-space char == '#') are
 *     skipped.
 *   - Leading and trailing ASCII whitespace is stripped.
 *   - A trailing '/' is stripped (directory-only marker; we match by
 *     name regardless of type).
 *   - Duplicate patterns (compared against the whole current list) are
 *     not added.
 *   - Patterns are heap-allocated; their range in opts->ignore[] is
 *     recorded via opts->gitignore_start so cli_opts_free() can free
 *     them precisely.
 */
static void cli_load_gitignore(cli_opts_t *opts) {
    /* Build path: root + "/.gitignore" */
    char gi_path[4096];
    size_t rlen = strlen(opts->root);
    /* sizeof("/.gitignore") includes the NUL, so this checks for overflow */
    if (rlen + sizeof("/.gitignore") >= sizeof(gi_path))
        return;

    memcpy(gi_path, opts->root, rlen);
    if (rlen == 0 || (opts->root[rlen - 1] != '/' && opts->root[rlen - 1] != '\\'))
        gi_path[rlen++] = '/';
    memcpy(gi_path + rlen, ".gitignore", sizeof(".gitignore")); /* includes NUL */

    FILE *f = fopen(gi_path, "r");
    if (!f) return;

    /*
     * Use a buffer large enough for practical gitignore patterns.
     * If fgets fills the buffer without finding '\n', the line is too
     * long — we discard it and drain the rest.
     */
    char line[512];
    int  overlong = 0; /* 1 while draining an overlong line */

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);

        /* Detect overlong lines: buffer full and no newline at end */
        if (len == sizeof(line) - 1 &&
            line[len - 1] != '\n' && line[len - 1] != '\r') {
            overlong = 1;
            continue;
        }
        if (overlong) {
            /* This chunk ends the overlong line — skip and reset */
            overlong = 0;
            continue;
        }

        /* Strip trailing newline / carriage-return */
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        /* Strip leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        /* Skip empty lines and comments */
        if (*p == '\0' || *p == '#') continue;

        /* Strip trailing whitespace */
        size_t plen = strlen(p);
        while (plen > 0 && (p[plen - 1] == ' ' || p[plen - 1] == '\t'))
            p[--plen] = '\0';

        /* Strip trailing '/' (directory-only marker) */
        if (plen > 0 && p[plen - 1] == '/')
            p[--plen] = '\0';

        if (plen == 0) continue;

        /* Skip duplicates — compare against the whole current list */
        int dup = 0;
        for (int i = 0; i < opts->ignore_count; i++) {
            if (strcmp(opts->ignore[i], p) == 0) { dup = 1; break; }
        }
        if (dup) continue;

        if (opts->ignore_count >= CLI_MAX_IGNORE) break;

        char *copy = malloc(plen + 1);
        if (!copy) break; /* OOM: stop loading, keep what we have */
        memcpy(copy, p, plen + 1);

        opts->ignore[opts->ignore_count++] = copy;
    }

    fclose(f);
}

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
        "  --ignore <name>       Ignore entries by name (repeatable)\n"
        "  --pattern <glob>      Show only files matching glob (e.g. *.c or **/*.h)\n"
        "  --all                 Show hidden files (dot-entries)\n"
        "  --dirs-only           Show directories only; summary omits file count\n"
        "  --size                Show file sizes in KB\n"
        "  --sort <key>          Sort entries: name (default), size, modified\n"
        "  --ext-summary         Print extension counts after the tree\n"
        "  --export-txt <file>   Write plain-text tree to file (no ANSI codes)\n"
        "  --export-markdown <file>  Write markdown (fenced) tree to file\n"
        "  --no-color            Disable ANSI color output\n"
        "  --no-dirs-first       Disable directory-first ordering\n"
        "  -h, --help            Show this help and exit\n"
        "  --version             Show version and exit\n"
        "\n"
        "Default ignored: .git, node_modules, __pycache__, target, build\n"
        ".gitignore in the root directory is loaded automatically.\n"
        "\n"
        "Glob patterns (--pattern):\n"
        "  *.c           match .c files in the current directory\n"
        "  **/*.c        match .c files at any depth\n"
        "  src/**/*.h    match .h files only under src/ (path-aware)\n"
        "  foo/**/bar.py match bar.py anywhere under foo/\n"
        "\n"
        "  Path-aware note: 'src/**/*.h' will NOT match './cli.h'.\n"
        "  Use '**/*.h' to match .h files everywhere.\n"
        "\n"
        "Export notes:\n"
        "  Export files are automatically excluded from the tree output.\n"
        "  Markdown export wraps the tree in a fenced code block.\n"
        "\n"
        "Examples:\n"
        "  neotree\n"
        "  neotree src/\n"
        "  neotree --all\n"
        "  neotree --dirs-only\n"
        "  neotree --size\n"
        "  neotree -L 2 .\n"
        "  neotree --pattern '*.c' src/\n"
        "  neotree --pattern '**/*.h'\n"
        "  neotree --pattern 'src/**/*.h' .\n"
        "  neotree --sort size\n"
        "  neotree --ext-summary\n"
        "  neotree --export-txt tree.txt\n"
        "  neotree --export-markdown tree.md\n"
        "  neotree --ignore dist --ignore .cache .\n"
        "  neotree --no-color | tee tree.txt\n"
    );
    exit(0);
}

void cli_parse(int argc, char *argv[], cli_opts_t *opts) {
    /* --- defaults --- */
    opts->root           = ".";
    opts->max_depth      = -1;
    opts->pattern        = NULL;
    opts->no_color       = 0;
    opts->dirs_first     = 1;
    opts->show_all       = 0;
    opts->dirs_only      = 0;
    opts->show_size      = 0;
    opts->sort_by        = SORT_NAME;
    opts->ext_summary    = 0;
    opts->export_txt     = NULL;
    opts->export_md      = NULL;
    opts->ignore_count   = 0;
    opts->gitignore_start = 0; /* will be set after argv loop */

    /* Seed ignore list with compile-time defaults (static — not freed) */
    for (int i = 0; i < DEFAULT_IGNORE_COUNT; i++) {
        if (opts->ignore_count < CLI_MAX_IGNORE)
            opts->ignore[opts->ignore_count++] = DEFAULT_IGNORE[i];
    }

    int end_of_flags = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (end_of_flags || arg[0] != '-') {
            opts->root = arg;
            continue;
        }

        if (strcmp(arg, "--") == 0) { end_of_flags = 1; continue; }

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
            cli_usage();

        if (strcmp(arg, "--version") == 0) {
            printf("%s %s\n", CLI_PROGRAM, CLI_VERSION);
            exit(0);
        }

        if (strcmp(arg, "-L") == 0) {
            if (i + 1 >= argc) die_usage("-L requires a depth argument", NULL);
            char *end;
            long v = strtol(argv[++i], &end, 10);
            if (*end != '\0' || v < 1)
                die_usage("-L depth must be a positive integer", argv[i]);
            opts->max_depth = (int)v;
            continue;
        }

        if (strcmp(arg, "--ignore") == 0) {
            if (i + 1 >= argc) die_usage("--ignore requires an argument", NULL);
            if (opts->ignore_count >= CLI_MAX_IGNORE)
                die_usage("too many --ignore entries (max 64)", NULL);
            /* argv pointer — not heap-allocated, must not be freed */
            opts->ignore[opts->ignore_count++] = argv[++i];
            continue;
        }

        if (strcmp(arg, "--pattern") == 0) {
            if (i + 1 >= argc) die_usage("--pattern requires an argument", NULL);
            opts->pattern = argv[++i];
            continue;
        }

        if (strcmp(arg, "--all") == 0)       { opts->show_all  = 1; continue; }
        if (strcmp(arg, "--dirs-only") == 0) { opts->dirs_only = 1; continue; }
        if (strcmp(arg, "--size") == 0)      { opts->show_size = 1; continue; }
        if (strcmp(arg, "--no-color") == 0)  { opts->no_color  = 1; continue; }
        if (strcmp(arg, "--no-dirs-first") == 0) { opts->dirs_first = 0; continue; }
        if (strcmp(arg, "--ext-summary") == 0)   { opts->ext_summary = 1; continue; }

        if (strcmp(arg, "--sort") == 0) {
            if (i + 1 >= argc) die_usage("--sort requires an argument", NULL);
            const char *key = argv[++i];
            if (strcmp(key, "name") == 0)         opts->sort_by = SORT_NAME;
            else if (strcmp(key, "size") == 0)    opts->sort_by = SORT_SIZE;
            else if (strcmp(key, "modified") == 0) opts->sort_by = SORT_MODIFIED;
            else die_usage("--sort must be 'name', 'size', or 'modified'", key);
            continue;
        }

        if (strcmp(arg, "--export-txt") == 0) {
            if (i + 1 >= argc) die_usage("--export-txt requires a filename", NULL);
            opts->export_txt = argv[++i];
            continue;
        }

        if (strcmp(arg, "--export-markdown") == 0) {
            if (i + 1 >= argc) die_usage("--export-markdown requires a filename", NULL);
            opts->export_md = argv[++i];
            continue;
        }

        die_usage("unknown option", arg);
    }

    /*
     * Record where the argv-sourced entries end.
     * cli_load_gitignore() appends after this index; cli_opts_free()
     * frees exactly those entries.
     */
    opts->gitignore_start = opts->ignore_count;

    /* Merge .gitignore patterns from root directory */
    cli_load_gitignore(opts);
}

void cli_opts_free(cli_opts_t *opts) {
    /*
     * Free only the heap-allocated .gitignore entries.
     * Entries [0, gitignore_start) are either static string literals
     * (DEFAULT_IGNORE) or argv pointers — both must NOT be freed.
     */
    for (int i = opts->gitignore_start; i < opts->ignore_count; i++) {
        free((char *)opts->ignore[i]);
        opts->ignore[i] = NULL;
    }
    opts->ignore_count    = opts->gitignore_start;
    opts->gitignore_start = 0;
}
