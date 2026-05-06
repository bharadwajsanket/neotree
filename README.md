<div align="center">

# 🌲 neotree

**A fast, minimal, developer-focused directory tree CLI — written in C.**

[![Version](https://img.shields.io/badge/version-v0.3.0-6C63FF?style=flat-square)](https://github.com/bharadwajsanket/neotree/releases)
[![Language](https://img.shields.io/badge/language-C99-00ADD8?style=flat-square&logo=c)](https://en.wikipedia.org/wiki/C99)
[![License](https://img.shields.io/badge/license-MIT-22C55E?style=flat-square)](./LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat-square)](#platform-support)
[![Status](https://img.shields.io/badge/status-active-success?style=flat-square)](#)

<br/>

> Built for developers who need **focused directory exploration** — not just a static tree dump.  
> Fast. Lightweight. No dependencies. UNIX-native.

<br/>

```
$ neotree --pattern 'src/**/*.c' --sort size .

.
└── src/
    ├── fs.c      (6 KB)
    ├── cli.c     (13 KB)
    ├── utils.c   (13 KB)
    └── tree.c    (15 KB)

1 directory, 4 files
```

</div>

---

## ✨ Features

| Feature | Description |
|---|---|
| 🎨 **Filetype colors** | Syntax-aware ANSI coloring by extension — `.c`, `.py`, `.md`, images, scripts |
| 🔍 **Glob filtering** | `*.c`, `**/*.h`, `src/**/*.h` — path-aware recursive matching |
| 🗂️ **Sorting** | Sort by `name`, `size`, or `modified` — within directory-first groups |
| 📊 **Extension summary** | Count files by extension across the whole tree |
| 📄 **Export** | Write clean plain-text or fenced markdown tree to a file |
| 🙈 **Ignore rules** | `--ignore`, `.gitignore` auto-loading, default ignores |
| 👁️ **Hidden files** | Toggle dot-entries with `--all` |
| 📁 **Dirs only** | `--dirs-only` shows just the structure, no file noise |
| 📏 **Depth limit** | `-L <n>` stops recursion at any level |
| ⚡ **Single-pass** | Each directory is read exactly once — no redundant I/O |
| 🔧 **Zero dependencies** | Pure C99, no libraries, no runtime, no config |
| 🪟 **Cross-platform** | Linux, macOS, and Windows — one codebase |

---

## 📦 Installation

### Linux / macOS

```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

<details>
<summary>Pin a specific version</summary>

```bash
VERSION=v0.3.0 curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

</details>

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

<details>
<summary>Pin a specific version</summary>

```powershell
$env:VERSION="v0.3.0"; irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

</details>

### Build from Source

```bash
git clone https://github.com/bharadwajsanket/neotree
cd neotree
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 \
    main.c tree.c fs.c cli.c utils.c \
    -o neotree
```

> Requires only a C99-compatible compiler (`gcc` or `clang`). No other dependencies.

### Uninstall

```bash
# Linux / macOS
sudo rm /usr/local/bin/neotree

# Windows (PowerShell)
Remove-Item "$env:LOCALAPPDATA\Programs\neotree\neotree.exe"
```

---

## 🚀 Quick Start

```bash
neotree                            # tree of current directory
neotree src/                       # tree of specific path
neotree --all                      # include hidden dot-files
neotree --dirs-only                # structure only, no files
neotree --size                     # show file sizes in KB
neotree -L 2                       # limit to 2 levels deep
neotree --sort size                # sort by file size
neotree --pattern "*.c"            # only .c files
neotree --pattern "**/*.h"         # all headers, any depth
neotree --pattern "src/**/*.h"     # headers only under src/
neotree --ext-summary              # show extension breakdown
neotree --export-txt tree.txt      # save as plain text
neotree --export-markdown tree.md  # save as markdown
neotree --no-color | tee tree.txt  # pipe-safe output
```

---

## 📖 Usage

```
neotree [OPTIONS] [PATH]
```

### Options

| Flag | Default | Description |
|---|---|---|
| `PATH` | `.` | Root directory to display |
| `-L <depth>` | unlimited | Limit display depth (`-L 1` = root entries only) |
| `--pattern <glob>` | — | Show only files matching glob pattern |
| `--ignore <name>` | — | Ignore entry by name; repeatable |
| `--all` | off | Show hidden files and directories (dot-entries) |
| `--dirs-only` | off | Show directories only; summary omits file count |
| `--size` | off | Show file size in KB next to each file |
| `--sort <key>` | `name` | Sort: `name`, `size`, or `modified` |
| `--ext-summary` | off | Print extension counts after the tree |
| `--export-txt <file>` | — | Write plain-text tree (no ANSI codes) |
| `--export-markdown <file>` | — | Write fenced markdown tree |
| `--no-color` | off | Disable ANSI color output |
| `--no-dirs-first` | off | Disable directory-first ordering |
| `-h`, `--help` | — | Show help and exit |
| `--version` | — | Show version and exit |

---

## 🔍 Glob Patterns

neotree supports two levels of glob matching via `--pattern`:

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

### Path-aware semantics

```
Pattern: src/**/*.h

  ✅  src/foo.h
  ✅  src/internal/bar.h
  ✅  src/a/b/c/baz.h

  ❌  cli.h               (not under src/)
  ❌  include/config.h    (not under src/)
```

> **Always quote glob patterns** in the shell: `--pattern "**/*.h"` not `--pattern **/*.h`

**Limitations:**
- No `?` or character-class (`[abc]`) wildcards
- Path separators in patterns must be `/` (even on Windows)
- Not full POSIX glob / bash brace expansion emulation

---

## 🗂️ Sorting

Sort entries within their groups (directories-first by default):

```bash
neotree --sort name      # A → Z alphabetical (default)
neotree --sort size      # smallest files first
neotree --sort modified  # most recently modified first
```

Combine with `--no-dirs-first` to sort everything flat:

```bash
neotree --sort modified --no-dirs-first .
```

---

## 📊 Extension Summary

```bash
neotree --ext-summary src/
```

```
src/
├── cli.c
├── cli.h
├── main.c
├── tree.c
├── tree.h
├── utils.c
└── utils.h

0 directories, 7 files

Extension summary:
  .c            4
  .h            3
```

Respects all active filters (`--pattern`, `--ignore`, `--all`).

---

## 📄 Export

Write the tree to a file without ANSI codes:

```bash
# Plain text
neotree --export-txt tree.txt .

# Markdown (wrapped in a fenced code block)
neotree --export-markdown tree.md .

# Both simultaneously
neotree --export-txt tree.txt --export-markdown tree.md .
```

**Notes:**
- Export files are **automatically excluded** from the tree output — `tree.md` will not appear inside the tree written to `tree.md`
- Exports contain no ANSI codes regardless of terminal state
- Compatible with `--ext-summary`, `--sort`, `--pattern`, and all other flags

---

## 🙈 Ignore Rules

### Default ignores (built-in)

```
.git    node_modules    __pycache__    target    build
```

### CLI ignores

```bash
neotree --ignore dist
neotree --ignore dist --ignore .cache --ignore "*.log"
```

### `.gitignore` auto-loading

If a `.gitignore` exists in the scanned root, neotree reads it automatically.

Rules supported (minimal subset — not the full gitignore spec):

- `#` comments and empty lines → skipped
- Trailing `/` → stripped (matched by name regardless of entry type)
- Exact name matches → `build`, `.DS_Store`
- Extension globs → `*.o`, `*.log`

> `.gitignore` rules apply even with `--all`. The `--all` flag only controls the hidden-file filter, not the ignore list.

---

## 🎨 Colors

File entries are colored by type when writing to a terminal:

| Color | Types |
|---|---|
| **Bold blue** | Directories |
| **Cyan** | `.c`, `.h`, `.cpp`, `.hpp`, `.cc`, `.cxx` |
| **Yellow** | `.py` |
| **Green** | `.md`, `.txt`, `.rst` |
| **Magenta** | `.sh`, `.bash`, `.zsh`, `.fish`, `.html`, `.css`, `.js`, `.ts` |
| **Purple** | `.png`, `.jpg`, `.jpeg`, `.gif`, `.webp`, `.svg`, `.bmp` |
| **Cyan** | `.json`, `.toml`, `.yaml`, `.yml` |
| **Red** | Executable files (POSIX exec bit set) |

Color is **automatically disabled** when stdout is piped — no need for `--no-color`.

---

## 🖥️ Platform Support

| Platform | Status | Install |
|---|---|---|
| **Linux** | ✅ Fully supported | `install.sh` |
| **macOS** | ✅ Fully supported | `install.sh` |
| **Windows** | ✅ Prebuilt `.exe`, no admin required | `install.ps1` |

Prebuilt binaries for every release:  
**→ [github.com/bharadwajsanket/neotree/releases](https://github.com/bharadwajsanket/neotree/releases)**

| Platform | Binary filename |
|---|---|
| Linux (x86_64) | `neotree-linux` |
| macOS | `neotree-macos` |
| Windows (x86_64) | `neotree-windows.exe` |

**macOS note:** System headers live under the Xcode SDK path, not `/usr/include`.  
If `/usr/include` is absent, neotree correctly reports it as missing — this is expected macOS behavior.

---

## 🏗️ Project Structure

```
neotree/
├── main.c        entry point — init, validate, walk, summarise, export
├── cli.c / cli.h argument parsing, .gitignore loading, defaults
├── fs.c  / fs.h  portable filesystem abstraction (POSIX + Win32)
├── utils.c / utils.h  colors, glob engine, sorting, ext-summary table
└── tree.c / tree.h    directory traversal and rendering
```

**Architecture principles:**
- No heap-allocated globals — all state flows through function parameters
- Single-pass traversal — each directory opened exactly once
- Dual-stream rendering — one pass writes terminal output + export file simultaneously
- Platform layer isolated in `fs.c` — POSIX and Win32 behind one interface

---

## 🗺️ Roadmap

- [x] `.gitignore`-aware filtering
- [x] `--all` hidden file toggle
- [x] `--dirs-only` mode
- [x] `--size` file size display
- [x] Filetype-based ANSI colors
- [x] `--sort name|size|modified`
- [x] `--ext-summary` extension breakdown
- [x] `--export-txt` / `--export-markdown`
- [x] Recursive glob (`**/*.c`, `src/**/*.h`) with path-aware matching
- [x] Export self-exclusion (no self-reference in generated tree)
- [x] Prebuilt binaries via GitHub Actions CI
- [ ] `--stats` aggregate size totals
- [ ] Windows path separator polish in patterns
- [ ] JSON output mode

---

## 🤝 Contributing

This project is intentionally small and hackable.  
The entire codebase is ~5 files, ~1200 lines of C.

**If something breaks** → open an issue  
**If something feels missing** → build it  
**If you can improve UX or performance** → send a PR

Good first contributions:
- Edge cases in `.gitignore` parsing
- New output modes (JSON, CSV)
- Windows path separator polish
- Cross-platform test cases

No strict process — keep it clean and readable.

---

## 📜 License

MIT © [Sanket Bharadwaj](https://github.com/bharadwajsanket)
