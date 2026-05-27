<div align="center">

# neotree

**A fast, zero-dependency tree(1) utility for developers.**

[![Version](https://img.shields.io/badge/version-v0.4.0-6C63FF?style=flat-square)](https://github.com/bharadwajsanket/neotree/releases)
[![Language](https://img.shields.io/badge/language-C99-00ADD8?style=flat-square&logo=c)](https://en.wikipedia.org/wiki/C99)
[![License](https://img.shields.io/badge/license-GPL%20v3.0-blue?style=flat-square)](./LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat-square)](#installation)

</div>

```
$ neotree --pattern 'src/**/*.c' --size --sort size .

.
└── src/
    ├── fs.c      (6 KB)
    ├── cli.c     (13 KB)
    ├── utils.c   (13 KB)
    └── tree.c    (15 KB)

1 directory, 4 files
```

---

## Features

| Feature | Description |
|---|---|
| **Filetype colors** | Syntax-aware ANSI coloring by extension — `.c`, `.py`, `.md`, images, scripts |
| **Glob filtering** | `*.c`, `**/*.h`, `src/**/*.h` — path-aware recursive matching |
| **Sorting** | Sort by `name`, `size`, or `modified` — within directory-first groups |
| **Reverse sort** | Flip sorting direction with `--reverse` |
| **Traversal stats** | Summary of directories, files, total size, and extensions with `--stats` |
| **Find mode** | Fast, isolated path lookups for files (`--find`) or directories (`--find-dir`) |
| **Export** | Write clean plain-text or fenced markdown tree to a file |
| **Ignore rules** | `--ignore`, `.gitignore` auto-loading, built-in defaults, comma-separated lists |
| **Dirs only** | `--dirs-only` shows just the structure, no file noise |
| **Depth limit** | `-L <n>` stops recursion at any level |
| **ntree alias** | Runs identically under `neotree` or `ntree` |
| **Zero dependencies** | Pure C99, no libraries, no runtime, no config |

---

## Philosophy

`neotree` is designed to be a focused utility that sits comfortably in a standard UNIX pipeline.

It explicitly focuses on:
- Fast, low-overhead directory traversal.
- Composable command-line options.
- Developer-oriented filtering (such as automatic `.gitignore` parsing).
- Human-readable, structured outputs (with optional markdown/txt exports).
- Minimal footprint (pure C99, zero dependencies).

It intentionally avoids:
- Interactive TUI or ncurses-based interfaces.
- Fuzzy searching or matching.
- Complex configuration files or plugin systems.

---

## Installation

### Homebrew (macOS / Linux)

```bash
brew tap bharadwajsanket/neotree && brew install neotree
```

<details>
<summary>Uninstallation</summary>

```bash
brew uninstall neotree && brew untap bharadwajsanket/neotree
```

</details>

### Linux / macOS (curl)

```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

<details>
<summary>Pin a specific version</summary>

```bash
VERSION=v0.4.0 bash <(curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh)
```

</details>

<details>
<summary>Uninstallation</summary>

```bash
sudo rm -f /usr/local/bin/neotree /usr/local/bin/ntree /usr/local/share/man/man1/neotree.1 /usr/local/share/man/man1/ntree.1
```

</details>

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

<details>
<summary>Pin a specific version</summary>

```powershell
$env:VERSION="v0.4.0"; irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

</details>

<details>
<summary>Uninstallation</summary>

```powershell
Remove-Item -Recurse -Force "$env:LOCALAPPDATA\Programs\neotree"
# Note: Remove the directory from your PATH environment variable if needed.
```

</details>

### Build from Source

```bash
git clone https://github.com/bharadwajsanket/neotree && cd neotree && make && sudo make install
```

<details>
<summary>Uninstallation</summary>

```bash
sudo make uninstall
```

</details>

---

## Quick Start

```bash
neotree                            # tree of current directory
neotree src/                       # tree of specific path
neotree src tests docs             # tree of multiple paths
neotree --all                      # include hidden dot-files
neotree --dirs-only                # structure only, no files
neotree --size                     # show file sizes in KB
neotree -L 2                       # limit to 2 levels deep
neotree --sort size --reverse      # sort by size descending
neotree --stats                    # show traversal statistics
neotree --find '*.c,src/**/*.h'    # search for files by pattern
neotree --find-dir 'test'          # search for directories
neotree --ignore 'dist,tmp,.cache' # ignore multiple patterns
neotree --export-txt tree.txt      # save as plain text
neotree --export-markdown tree.md  # save as markdown
```

---

## Usage

```
neotree [OPTIONS] [PATH...]
```

### Options

| Flag | Default | Description |
|---|---|---|
| `PATH...` | `.` | Root directories to display (sequentially rendered) |
| `-L <depth>` | unlimited | Limit display depth (`-L 1` = root entries only) |
| `--pattern <glob>` | — | Show only files matching glob pattern |
| `--ignore <patterns>` | — | Ignore entries by name (comma-separated or repeatable) |
| `--all` | off | Show hidden files and directories (dot-entries) |
| `--dirs-only` | off | Show directories only |
| `--size` | off | Show file size in KB next to each file |
| `--sort <key>` | `name` | Sort: `name`, `size`, or `modified` |
| `--reverse` | off | Reverse sorting order (requires `--sort`) |
| `--stats` | off | Print traversal statistics and extension breakdown |
| `--find <query>` | — | Isolated file path lookup only (comma-separated exact/glob) |
| `--find-dir <query>`| — | Isolated directory path lookup only (comma-separated exact/glob) |
| `--export-txt <file>` | — | Write plain-text tree (no ANSI codes) |
| `--export-markdown <file>` | — | Write fenced markdown tree |
| `--no-color` | off | Disable ANSI color output |
| `--no-dirs-first` | off | Disable directory-first ordering |
| `-h`, `--help` | — | Show help and exit |
| `--version` | — | Show version and exit |

### Manual Pages

Full offline documentation is available via manual pages:

```bash
man neotree
man ntree
```

Manual pages are installed automatically via Homebrew, install.sh, and make install.

---

## Glob Patterns

neotree supports glob matching via `--pattern`, `--find`, and `--find-dir`:

### Simple patterns (filename only)

```bash
neotree --pattern "*.c"      # .c files in current directory only
neotree --pattern "Makefile" # exact name match
```

### Recursive patterns (`**`)

```bash
neotree --pattern "**/*.c"        # all .c files at any depth
neotree --pattern "**/*.h"        # all .h files at any depth
neotree --pattern "src/**/*.h"    # .h files only under src/
neotree --pattern "lib/**/test.c" # test.c anywhere under lib/
```

---

## Sorting & Reversing

Sort entries within their groups (directories-first by default):

```bash
neotree --sort name      # A → Z alphabetical (default)
neotree --sort size      # smallest files first
neotree --sort modified  # oldest files first
```

Combine with `--reverse` to invert the order:

```bash
neotree --sort size --reverse     # largest files first
neotree --sort modified --reverse # most recently modified first
```

> **Note:** `--reverse` is invalid without `--sort` and will show an error.

---

## Traversal Statistics

Using `--stats` displays traversal totals and an extension breakdown after the tree:

```bash
neotree --stats .
```

```
.
├── cli.c
├── cli.h
├── main.c
└── tree.c

0 directories, 4 files

Stats:
  directories      0
  files            4
  total size       42.5 KB

Extensions:
  .c               3
  .h               1
```

If `--dirs-only` is active, file-size-based stats and extension breakdowns are hidden:

```
Stats:
  directories      12
  max depth        4
```

---

## Find Mode

Find mode is an isolated path lookup tool (`--find` or `--find-dir`). It does not render the tree, sort, ignore entries, or count statistics. It simply outputs matching paths on newlines:

```bash
neotree --find 'tree.c'
./src/tree.c
./lib/tree.c

neotree --find-dir 'src,test'
./src
./test
```

Anchor searches to the root using a leading `/`:

```bash
neotree --find '/*.c'    # .c files in the root directory only
./main.c
```

---

## Project Structure

```
neotree/
├── main.c          entry point — CLI logic flow and export streams
├── cli.c / cli.h   argument parsing, .gitignore loading, defaults
├── fs.c  / fs.h    portable filesystem abstraction (POSIX + Win32)
├── find.c / find.h isolated lookup traversal and query matching
├── utils.c / utils.h  colors, glob engine, sorting helper
└── tree.c / tree.h    directory traversal and tree rendering
```

---

## License

This project is licensed under the GNU GPL v3.0 License.

Copyright (C) 2025 Sanket Bharadwaj
