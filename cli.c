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
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_prog_name = "neotree";

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
void cli_load_gitignore(cli_opts_t *opts, const char *root_path) {
    /* Build path: root_path + "/.gitignore" */
    char gi_path[4096];
    size_t rlen = strlen(root_path);
    /* sizeof("/.gitignore") includes the NUL, so this checks for overflow */
    if (rlen + sizeof("/.gitignore") >= sizeof(gi_path))
        return;

    memcpy(gi_path, root_path, rlen);
    if (rlen == 0 || (root_path[rlen - 1] != '/' && root_path[rlen - 1] != '\\'))
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

        if (opts->ignore_count >= CLI_MAX_IGNORE) {
            fprintf(stderr, "neotree: warning: ignore list full (max %d), some .gitignore patterns skipped\n", CLI_MAX_IGNORE);
            break;
        }

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
        fprintf(stderr, "%s: %s: '%s'\n", g_prog_name, msg, arg);
    else
        fprintf(stderr, "%s: %s\n", g_prog_name, msg);
    fprintf(stderr, "Try '%s --help' for usage.\n", g_prog_name);
    exit(1);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void cli_usage(void) {
    printf(
        "============================================================\n"
        "%s v2.5.4\n"
        "Fast, modern directory tree for developers.\n"
        "============================================================\n"
        "\n"
        "USAGE\n"
        "-----\n"
        "  %s [OPTIONS] [PATH...]\n"
        "\n"
        "PATH\n"
        "----\n"
        "  [PATH...]                   Directories to display (default: .)\n"
        "\n"
        "DISPLAY\n"
        "-------\n"
        "  -L <depth>                  Limit display depth (1 = root entries only)\n"
        "  --all                       Show hidden files (dot-entries)\n"
        "  --dirs-only                 Show directories only\n"
        "  --size                      Show file sizes next to each file\n"
        "  --sort <name|size|modified> Sort entries by key (default: name)\n"
        "  --reverse                   Reverse the active sort order (requires --sort)\n"
        "  --no-dirs-first             Disable directory-first ordering\n"
        "  --no-color                  Disable ANSI color output\n"
        "\n"
        "FILTERING\n"
        "---------\n"
        "  --pattern <glob>            Filter files matching glob pattern\n"
        "  --ignore <patterns>         Ignore entries by name (comma-separated)\n"
        "\n"
        "ANALYSIS\n"
        "--------\n"
        "  --stats                     Print statistics (files, size, deepest path, etc.)\n"
        "                              Includes:\n"
        "                              * traversal statistics\n"
        "                              * extension breakdown\n"
        "                              * largest file\n"
        "                              * largest directory\n"
        "                              * deepest path\n"
        "  --largest <N>               Show top N largest files (standalone mode)\n"
        "  --largest-dirs <N>          Show top N largest directories by recursive size\n"
        "\n"
        "SEARCH\n"
        "------\n"
        "  --find <query>              Isolated file search (comma-separated, glob-aware)\n"
        "  --find-dir <query>          Isolated directory search (comma-separated)\n"
        "\n"
        "EXPORT\n"
        "------\n"
        "  --export-txt <file>         Write plain-text tree to file\n"
        "  --export-markdown <file>    Write fenced markdown tree to file\n"
        "  --export-json <file>        Write RFC 8259 compliant JSON tree to file\n"
        "\n"
        "GENERAL\n"
        "-------\n"
        "  -h, --help                  Show this help and exit\n"
        "  --version                   Show version and exit\n"
        "\n"
        "DEFAULT IGNORE\n"
        "--------------\n"
        "  .git\n"
        "  node_modules\n"
        "  __pycache__\n"
        "  build\n"
        "  target\n"
        "\n"
        "EXAMPLES\n"
        "--------\n"
        "  %s                           Current directory\n"
        "  %s src                       Walk 'src' directory\n"
        "  %s --stats                   Print statistics for current directory\n"
        "  %s --largest 10              Print top 10 largest files\n"
        "  %s --largest-dirs 10         Print top 10 largest directories\n"
        "  %s --pattern \"**/*.h\"        Display only header files at any depth\n"
        "  %s --find \"*.c\"              Find all C files recursively\n"
        "  %s --export-json tree.json   Export tree as JSON to 'tree.json'\n"
        "\n"
        "GitHub\n"
        "------\n"
        "  https://github.com/bharadwajsanket/neotree\n",
        g_prog_name, g_prog_name, g_prog_name, g_prog_name, g_prog_name, g_prog_name,
        g_prog_name, g_prog_name, g_prog_name, g_prog_name
    );
    exit(0);
}

void cli_parse(int argc, char *argv[], cli_opts_t *opts) {
    /* --- argv[0] detection for alias support --- */
    if (argc > 0 && argv[0]) {
        const char *base = utils_basename(argv[0]);
        static char detected_name[128];
        strncpy(detected_name, base, sizeof(detected_name) - 1);
        detected_name[sizeof(detected_name) - 1] = '\0';
        char *dot = strrchr(detected_name, '.');
        if (dot && (strcmp(dot, ".exe") == 0 || strcmp(dot, ".EXE") == 0)) {
            *dot = '\0';
        }
        if (strcmp(detected_name, "ntree") == 0 || strcmp(detected_name, "neotree") == 0) {
            g_prog_name = detected_name;
        }
    }

    /* --- defaults --- */
    opts->roots_count    = 0;
    opts->max_depth      = -1;
    opts->pattern        = NULL;
    opts->no_color       = 0;
    opts->dirs_first     = 1;
    opts->show_all       = 0;
    opts->dirs_only      = 0;
    opts->show_size      = 0;
    opts->sort_by        = SORT_NAME;
    opts->show_stats     = 0;
    opts->reverse        = 0;
    opts->sort_explicit  = 0;
    opts->find           = NULL;
    opts->find_dir       = NULL;
    opts->export_txt     = NULL;
    opts->export_md      = NULL;
    opts->export_json    = NULL;
    opts->largest_n      = 0;
    opts->largest_dirs_n = 0;
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
            if (opts->roots_count < CLI_MAX_ROOTS) {
                opts->roots[opts->roots_count++] = arg;
            } else {
                die_usage("too many root paths (max 64)", NULL);
            }
            continue;
        }

        if (strcmp(arg, "--") == 0) { end_of_flags = 1; continue; }

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
            cli_usage();

        if (strcmp(arg, "--version") == 0) {
            printf("%s %s\n", g_prog_name, CLI_VERSION);
            exit(0);
        }

        if (strcmp(arg, "-L") == 0) {
            if (i + 1 >= argc) die_usage("-L requires a depth argument", NULL);
            char *end;
            long v = strtol(argv[++i], &end, 10);
            if (*end != '\0' || v < 1 || v > 10000)
                die_usage("-L depth must be a positive integer between 1 and 10000", argv[i]);
            opts->max_depth = (int)v;
            continue;
        }

        if (strcmp(arg, "--ignore") == 0) {
            if (i + 1 >= argc) die_usage("--ignore requires an argument", NULL);
            char *arg_val = argv[++i];
            char *tok = arg_val;
            while (tok) {
                char *comma = strchr(tok, ',');
                if (comma) {
                    *comma = '\0';
                }
                if (opts->ignore_count >= CLI_MAX_IGNORE) {
                    die_usage("too many --ignore entries (max 64)", NULL);
                }
                opts->ignore[opts->ignore_count++] = tok;
                if (comma) {
                    tok = comma + 1;
                } else {
                    break;
                }
            }
            continue;
        }

        if (strcmp(arg, "--pattern") == 0) {
            if (i + 1 >= argc) die_usage("--pattern requires an argument", NULL);
            char *pat = argv[++i];
            for (char *p = pat; *p; p++) {
                if (*p == '\\') {
                    *p = '/';
                }
            }
            opts->pattern = pat;
            continue;
        }

        if (strcmp(arg, "--all") == 0)       { opts->show_all  = 1; continue; }
        if (strcmp(arg, "--dirs-only") == 0) { opts->dirs_only = 1; continue; }
        if (strcmp(arg, "--size") == 0)      { opts->show_size = 1; continue; }
        if (strcmp(arg, "--no-color") == 0)  { opts->no_color  = 1; continue; }
        if (strcmp(arg, "--no-dirs-first") == 0) { opts->dirs_first = 0; continue; }
        if (strcmp(arg, "--stats") == 0)         { opts->show_stats = 1; continue; }
        if (strcmp(arg, "--reverse") == 0)       { opts->reverse = 1; continue; }

        if (strcmp(arg, "--find") == 0) {
            if (i + 1 >= argc) die_usage("--find requires an argument", NULL);
            opts->find = argv[++i];
            continue;
        }

        if (strcmp(arg, "--find-dir") == 0) {
            if (i + 1 >= argc) die_usage("--find-dir requires an argument", NULL);
            opts->find_dir = argv[++i];
            continue;
        }

        if (strcmp(arg, "--sort") == 0) {
            if (i + 1 >= argc) die_usage("--sort requires an argument", NULL);
            const char *key = argv[++i];
            opts->sort_explicit = 1;
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

        if (strcmp(arg, "--export-json") == 0) {
            if (i + 1 >= argc) die_usage("--export-json requires a filename", NULL);
            opts->export_json = argv[++i];
            continue;
        }

        if (strcmp(arg, "--largest") == 0) {
            if (i + 1 >= argc) die_usage("--largest requires a count", NULL);
            {
                char *end;
                long v = strtol(argv[++i], &end, 10);
                if (*end != '\0' || v < 1 || v > 10000)
                    die_usage("--largest count must be 1..10000", argv[i]);
                opts->largest_n = (int)v;
            }
            continue;
        }

        if (strcmp(arg, "--largest-dirs") == 0) {
            if (i + 1 >= argc) die_usage("--largest-dirs requires a count", NULL);
            {
                char *end;
                long v = strtol(argv[++i], &end, 10);
                if (*end != '\0' || v < 1 || v > 10000)
                    die_usage("--largest-dirs count must be 1..10000", argv[i]);
                opts->largest_dirs_n = (int)v;
            }
            continue;
        }

        die_usage("unknown option", arg);
    }

    if (opts->reverse && !opts->sort_explicit) {
        die_usage("--reverse requires --sort", NULL);
    }

    /*
     * Append the basenames of active export targets to the ignore list.
     * Since these point to argv paths, they are not heap-allocated,
     * so we add them before setting gitignore_start so they are not
     * freed by cli_opts_free.
     */
    if (opts->export_txt && opts->ignore_count < CLI_MAX_IGNORE) {
        opts->ignore[opts->ignore_count++] = utils_basename(opts->export_txt);
    }
    if (opts->export_md && opts->ignore_count < CLI_MAX_IGNORE) {
        opts->ignore[opts->ignore_count++] = utils_basename(opts->export_md);
    }
    if (opts->export_json && opts->ignore_count < CLI_MAX_IGNORE) {
        opts->ignore[opts->ignore_count++] = utils_basename(opts->export_json);
    }

    /*
     * Record where the argv-sourced entries end.
     * cli_load_gitignore() appends after this index; cli_opts_free()
     * frees exactly those entries.
     */
    opts->gitignore_start = opts->ignore_count;

    if (opts->roots_count == 0) {
        opts->roots[opts->roots_count++] = ".";
    }
}

void cli_gitignore_free(cli_opts_t *opts) {
    for (int i = opts->gitignore_start; i < opts->ignore_count; i++) {
        free((char *)opts->ignore[i]);
        opts->ignore[i] = NULL;
    }
    opts->ignore_count = opts->gitignore_start;
}

void cli_opts_free(cli_opts_t *opts) {
    cli_gitignore_free(opts);
}
