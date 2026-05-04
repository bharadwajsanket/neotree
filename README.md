# neotree

![C](https://img.shields.io/badge/language-C99-blue)
![version](https://img.shields.io/badge/version-v0.2.0-blue)
![status](https://img.shields.io/badge/status-active-success)
![platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)
![license](https://img.shields.io/badge/license-MIT-green)

A fast, minimal tree CLI with filtering, ignore rules, and per-directory insights.

Built for developers who need fast, focused directory exploration — not just a static tree view.

---

## Quick Install

```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

---

## Why neotree?

The standard `tree` command shows you structure. That's where it stops.

No filtering. No ignore rules. No sense of what's actually inside each folder.

`neotree` is built around how developers actually explore projects — you want to see only the `.c` files, skip `node_modules`, and know at a glance which directories have content and which are empty.

## Why not `tree`?

`tree` is great for structure.  
`neotree` adds:

- filtering by extension or name
- smart ignore rules (with sane defaults, and `.gitignore` support)
- per-directory file counts and empty folder detection
- hidden file toggle, size display, and dirs-only mode

---

## Features

- 📁 Directory-first tree view
- 🔍 Pattern filtering (`--pattern "*.c"`)
- 🚫 Default + custom ignore rules (`--ignore`) with `.gitignore` support
- 👁  Hidden file toggle (`--all`)
- 📂 Directories-only mode (`--dirs-only`)
- 📦 File size display (`--size`)
- 📏 Depth limiting (`-L`)
- 🎨 Colored output with automatic TTY detection
- 📊 Direct file count per directory
- ⚠️ Empty folder detection
- ⚡ Fast and lightweight — written in C99, zero dependencies

---

## Example

```
$ neotree src/

src/
├── internal/  (1 file)
│   └── core.c
├── utils.c
└── utils.h

1 directory, 3 files
```

```
$ neotree --all --size .

.
├── .gitignore  (0 KB)
├── Makefile    (3 KB)
├── README.md   (4 KB)
├── cli.c       (10 KB)
├── main.c      (1 KB)
└── tree.c      (9 KB)

0 directories, 6 files
```

```
$ neotree --dirs-only .

.
├── src/  [empty]
└── include/  [empty]

2 directories, 0 files
```

---

## Installation

### Quick Install (Recommended)

Install the latest stable version:

```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

Install a specific version:

```bash
VERSION=v0.2.0 curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

The installer:
- requires no `git` — downloads a source tarball directly
- falls back to the `main` branch if the tag is not found
- uses `sudo` only for the install step if `/usr/local/bin` is not writable

---

### Manual Build

```bash
git clone https://github.com/bharadwajsanket/neotree
cd neotree
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 \
    main.c tree.c fs.c cli.c utils.c \
    -o neotree
```

**With Makefile:**

```bash
make
sudo make install   # installs to /usr/local/bin
```

---

### Uninstall

```bash
sudo rm /usr/local/bin/neotree
```

---

## Platform Support

| Platform | Status |
|---|---|
| Linux | ✅ Fully supported |
| macOS | ✅ Fully supported |
| Windows | ⚠️ Download `.exe` from [Releases](https://github.com/bharadwajsanket/neotree/releases) (source builds require a POSIX-compatible toolchain) |

---

## Releases

Prebuilt binaries are available on the [GitHub Releases page](https://github.com/bharadwajsanket/neotree/releases):

https://github.com/bharadwajsanket/neotree/releases

Download and run directly — no compiler required:

| Platform | File |
|---|---|
| Linux (x86_64) | `neotree-linux-x86_64` |
| macOS (x86_64 / arm64) | `neotree-macos` |
| Windows | `neotree.exe` |

---

## Usage

```bash
neotree                                    # current directory
neotree src/                               # specific path
neotree --all                              # show hidden files (dot-files)
neotree --dirs-only                        # show directories only
neotree --size                             # show file sizes in KB
neotree -L 2                               # limit to 2 levels deep
neotree --pattern "*.c"                    # only show .c files
neotree --ignore dist                      # skip a directory
neotree --ignore dist --ignore .cache      # multiple ignores
neotree --no-color | tee tree.txt          # pipe-safe plain output
neotree --no-dirs-first                    # alphabetical order (no grouping)
```

> 💡 Always quote patterns to prevent shell glob expansion.  
> Use `neotree --pattern "*.c"`, not `neotree --pattern *.c`.

---

## Options

| Flag | Description |
|---|---|
| `-L <depth>` | Limit display depth |
| `--all` | Show hidden files and directories (starting with `.`) |
| `--dirs-only` | Show directories only; files are counted but not printed |
| `--size` | Show file size in KB next to each file |
| `--pattern <glob>` | Show only files matching pattern (e.g. `*.c`, `*.py`) |
| `--ignore <name>` | Ignore a file or directory by name (repeatable) |
| `--no-color` | Disable ANSI colors |
| `--no-dirs-first` | Disable directory-first ordering |
| `--version` | Print version |
| `-h, --help` | Show help |

---

## .gitignore Support

If a `.gitignore` file exists in the root directory being scanned, neotree reads it automatically and merges the patterns into its ignore list.

Rules applied (intentionally minimal — not the full gitignore spec):

- Empty lines and comments (`#`) are skipped
- Trailing `/` is stripped (directory-only marker; neotree matches by name)
- Exact name matches (e.g. `build`, `.DS_Store`)
- Suffix glob matches (e.g. `*.o`, `*.log`)
- Duplicates are suppressed

> **Note:** `.gitignore` rules apply even when `--all` is used. `--all` only controls the hidden-file filter, not the ignore list.

---

## Notes

- Automatically ignores: `.git`, `node_modules`, `__pycache__`, `target`, `build`
- Color is auto-disabled when output is piped — no need to pass `--no-color` manually
- `[empty]` means the directory has no direct visible files (subdirectories may still exist)
- Symlinks are listed but never followed (no infinite recursion)
- Single-pass traversal per directory — each folder is read exactly once

---

## Project Structure

```
neotree/
├── main.c      entry point — parse, init, walk, summarise
├── cli.c/h     argument parsing + .gitignore loading
├── fs.c/h      portable filesystem abstraction (POSIX + Win32)
├── utils.c/h   colors, matching, dynamic entry array
└── tree.c/h    traversal and rendering
```

---

## Roadmap

- [x] `.gitignore`-aware filtering
- [x] `--all` hidden file toggle
- [x] `--dirs-only` mode
- [x] `--size` file size display
- [ ] Glob pattern improvements (`**/*.c`, character classes)
- [ ] `--stats` flag for aggregate size info
- [ ] Prebuilt binaries via GitHub Actions
- [ ] Windows native path separator polish

---

## License

MIT

---

## Author

Sanket Bharadwaj