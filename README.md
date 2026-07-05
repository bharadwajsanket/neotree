<div align="center">

# neotree

**A fast, minimal directory tree utility for developers.**

[![Version](https://img.shields.io/badge/version-v2.5.4-6C63FF?style=flat-square)](https://github.com/bharadwajsanket/neotree/releases)
[![CI](https://github.com/bharadwajsanket/neotree/actions/workflows/ci.yml/badge.svg)](https://github.com/bharadwajsanket/neotree/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-GPL%20v3.0-blue?style=flat-square)](./LICENSE)

</div>

```
$ neotree --stats src/

src/
├── cli.c    (16 KB)
├── cli.h     (3 KB)
├── find.c    (4 KB)
├── fs.c      (6 KB)
├── main.c    (8 KB)
├── tree.c   (15 KB)
└── utils.c  (14 KB)

0 directories, 7 files

Stats:
  directories      0
  files            7
  total size       66.0 KB

Extensions:
  .c               6
  .h               1
```

---

## Installation

**Homebrew** (macOS / Linux):
```bash
brew tap bharadwajsanket/neotree && brew install neotree
```

**Linux / macOS:**
```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

**Windows:**
```powershell
irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

**Build from source:**
```bash
git clone https://github.com/bharadwajsanket/neotree && cd neotree && make && sudo make install
```

See [Uninstalling](#uninstalling) for removal instructions.

---

## Quick start

```bash
neotree                             # current directory
neotree src/ tests/                 # multiple paths
neotree --all                       # include hidden files
neotree --dirs-only                 # structure only, no files
neotree --size                      # show file sizes
neotree -L 2                        # limit to 2 levels deep
neotree --sort size --reverse       # largest files first
neotree --stats                     # traversal statistics
neotree --find '*.c,**/*.h'         # find files by pattern
neotree --find-dir 'test'           # find directories
neotree --ignore 'dist,tmp,.cache'  # exclude entries
neotree --export-markdown tree.md   # save as markdown
```

---

## Philosophy

neotree is a focused UNIX utility. It does one thing and sits comfortably in a pipeline.

It focuses on:
- Fast, low-overhead directory traversal
- Zero external dependencies — pure C99, single binary
- Developer-oriented filtering: `--ignore`, `.gitignore` auto-loading, glob patterns
- Composable output: plain text and markdown export, plays well with pipes
- Readable results with no configuration required

It deliberately avoids interactive TUIs, plugin systems, fuzzy search, configuration files, and anything that conflicts with simple, predictable CLI behavior.

---

## Features

| Feature | Description |
|---|---|
| **Filetype colors** | ANSI coloring by extension — `.c`, `.py`, `.md`, images, shell scripts |
| **Glob filtering** | `*.c`, `**/*.h`, `src/**/*.h` — path-aware recursive matching |
| **Sorting** | Sort by `name`, `size`, or `modified` within directory-first groups |
| **Traversal stats** | File counts, total size, extension breakdown, largest file, largest dir, deepest path via `--stats` |
| **Find mode** | Isolated path lookups via `--find` and `--find-dir` |
| **Export** | Plain-text, fenced markdown, or structured JSON tree output |
| **Largest search** | Standalone mode to query top-N largest files or directories |
| **Ignore rules** | `--ignore`, `.gitignore` auto-loading, built-in defaults |
| **Depth limit** | `-L <n>` stops recursion at any level |
| **Zero dependencies** | Pure C99, no libraries, no runtime, no config |

---

## Usage

```
neotree [OPTIONS] [PATH...]
```

| Flag | Default | Description |
|---|---|---|
| `PATH...` | `.` | Directories to display (multiple, rendered sequentially) |
| `-L <depth>` | unlimited | Limit display depth (`-L 1` = root entries only) |
| `--pattern <glob>` | — | Show only files matching glob pattern |
| `--ignore <patterns>` | — | Ignore entries by name (comma-separated or repeatable) |
| `--all` | off | Show hidden files and directories |
| `--dirs-only` | off | Show directories only |
| `--size` | off | Show file sizes next to each file |
| `--sort <key>` | `name` | Sort: `name`, `size`, or `modified` (most recently modified first) |
| `--reverse` | off | Reverse sort order (requires `--sort`) |
| `--stats` | off | Print statistics (including largest file, largest dir, deepest path) |
| `--largest <N>` | — | Print top N largest files in descending order |
| `--largest-dirs <N>` | — | Print top N largest directories by recursive byte size |
| `--find <query>` | — | Isolated file search — comma-separated, glob-aware |
| `--find-dir <query>` | — | Isolated directory search — comma-separated, glob-aware |
| `--export-txt <file>` | — | Write plain-text tree (no ANSI codes) |
| `--export-markdown <file>` | — | Write fenced markdown tree |
| `--export-json <file>` | — | Write structured JSON tree |
| `--no-color` | off | Disable ANSI color output |
| `--no-dirs-first` | off | Disable directory-first ordering |
| `-h`, `--help` | — | Show help and exit |
| `--version` | — | Show version and exit |

Default ignored: `.git`, `node_modules`, `__pycache__`, `target`, `build`.
`.gitignore` in any target directory is loaded automatically.

---

## Glob patterns

Supported by `--pattern`, `--find`, and `--find-dir`.

```bash
neotree --pattern "*.c"           # .c files in the root only
neotree --pattern "**/*.c"        # all .c files at any depth
neotree --pattern "src/**/*.h"    # .h files only under src/
neotree --pattern "lib/**/test.c" # test.c anywhere under lib/
```

`**` crosses directory boundaries. A simple `*.ext` pattern is a filename-only match.

---

## Find mode

`--find` and `--find-dir` are isolated search tools — they print matching paths to stdout without rendering a tree. The ignore list is respected.

```bash
neotree --find 'main.c'           # exact name
neotree --find '*.c,**/*.h'       # comma-separated patterns
neotree --find '/*.c'             # anchored to root directory only
neotree --find-dir 'src,test'     # find directories
```

---

## Sorting

```bash
neotree --sort name               # A → Z (default)
neotree --sort size               # smallest first
neotree --sort modified           # most recently modified first
neotree --sort size --reverse     # largest first
neotree --sort modified --reverse # oldest first
```

`--reverse` requires `--sort`.

---

## JSON Export

With `--export-json <file>`, neotree writes a structured JSON representation of the traversed directory tree.

```json
{
  "name": "src",
  "path": "src",
  "type": "dir",
  "size": -1,
  "mtime": 0,
  "children": [
    {
      "name": "main.c",
      "path": "src/main.c",
      "type": "file",
      "size": 15729,
      "mtime": 1780958811,
      "children": null
    }
  ]
}
```

---

## Largest search (top-N)

Standalone search options to locate the largest files or directories. These bypass tree rendering and print sorted descending tables.

```bash
neotree --largest 5           # show top 5 largest files
neotree --largest-dirs 10     # show top 10 largest directories by recursive size
```

---

## Manual pages

```bash
man neotree
man ntree      # ntree is an alias for neotree
```

Installed automatically by Homebrew, `install.sh`, and `make install`.

---

## Development

```bash
make            # build ./neotree
make debug      # build with AddressSanitizer + UBSan
make test       # build and run the full test suite
sudo make install   # install to /usr/local (binary + manpage)
sudo make uninstall # remove installed files
make clean      # remove build artifacts
```

Run the manpage without installing:

```bash
man ./man/neotree.1
```

---

## Project structure

```
neotree/
├── main.c           entry point — argument handling, export streams, summary
├── cli.c / cli.h    argument parsing, .gitignore loading, defaults
├── fs.c  / fs.h     portable filesystem abstraction (POSIX + Win32)
├── find.c / find.h  isolated path search traversal
├── tree.c / tree.h  directory traversal and tree rendering
├── utils.c / utils.h  colors, glob matching, sorting, extension table
├── man/neotree.1    manpage (handwritten roff)
├── Makefile         build, install, test, clean targets
├── install.sh       Linux/macOS installer
└── install.ps1      Windows installer
```

---

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md).

---

## Uninstalling

**Homebrew:**
```bash
brew uninstall neotree && brew untap bharadwajsanket/neotree
```

**install.sh:**
```bash
# Run the remote installer in uninstall mode:
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash -s -- --uninstall
```

**make install:**
```bash
sudo make uninstall
```

**Windows:**
```powershell
# Run the remote installer in uninstall mode:
$env:UNINSTALL = $true; irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

---

## Notes, Caveats & Limitations

### Shell Wildcard Expansion
A common mistake is typing:
```bash
neotree --find *.c
```
Because shells expand wildcards (like `*.c` or `*.md`) before running the program, `neotree` will receive the expanded list of filenames as separate arguments instead of the raw pattern. This results in error messages like `neotree: 'file.c' is not a directory`.

**Solution:** Always quote your patterns:
```bash
neotree --find "*.c"
neotree --pattern "*.md"
```

### Architecture Support
The automatic installer (`install.sh`) supports:
- **macOS**: `x86_64` (Intel) and `arm64` (Apple Silicon).
- **Linux**: `x86_64` (amd64), `arm64` (`aarch64`), and `armv7` (`armhf`).
On unsupported architectures, the installer will print an instruction to compile from source.

### Windows Unicode & Path Limitations
Windows Terminal and PowerShell render UTF-8 box-drawing characters correctly because `neotree` configures the console to the UTF-8 code page (65001) at startup.
However, because Windows paths are parsed via ANSI standard APIs (`GetFileAttributesA` and `FindFirstFileA`), paths containing characters outside the local active ANSI code page are not supported and may fail to open or display placeholder (`?`) characters. For full Unicode path support, compiling on Linux/macOS or WSL is recommended.

---

## License

GNU GPL v3.0 — see [LICENSE](./LICENSE).

Copyright © 2025 Sanket Bharadwaj
