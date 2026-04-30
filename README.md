# neotree

![C](https://img.shields.io/badge/language-C99-blue)
![status](https://img.shields.io/badge/status-active-success)
![platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)
![license](https://img.shields.io/badge/license-MIT-green)

A fast, minimal tree CLI with filtering, ignore rules, and per-directory insights.

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
- smart ignore rules (with sane defaults)
- per-directory file counts and empty folder detection

---

## Features

- 📁 Directory-first tree view
- 🔍 Pattern filtering (`--pattern "*.c"`)
- 🚫 Default + custom ignore rules (`--ignore`)
- 📏 Depth limiting (`-L`)
- 🎨 Colored output with automatic TTY detection
- 📊 Direct file count per directory
- ⚠️ Empty folder detection
- ⚡ Fast and lightweight — written in C99, zero dependencies

---

## Example

```
$ neotree test_project

test_project
├── assets/  (2 files)
│   ├── logo.png
│   └── style.css
├── docs/  (2 files)
│   ├── guide.md
│   └── notes.txt
├── empty_folder/  [empty]
├── src/  (3 files)
│   ├── module1.c
│   ├── module2.c
│   └── utils.py
├── README.md
├── app.py
├── helper.h
├── index.html
├── main.c
└── script.js

4 directories, 13 files
```

---

## Installation

**One-liner:**

```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

**From source:**

```bash
git clone https://github.com/bharadwajsanket/neotree
cd neotree
gcc main.c tree.c fs.c cli.c utils.c -o neotree
```

**With Makefile:**

```bash
make
sudo make install   # installs to /usr/local/bin
```

---

## Usage

```bash
neotree                                    # current directory
neotree src/                               # specific path
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
| `--pattern <glob>` | Show only files matching pattern (e.g. `*.c`, `*.py`) |
| `--ignore <name>` | Ignore a file or directory by name (repeatable) |
| `--no-color` | Disable ANSI colors |
| `--no-dirs-first` | Disable directory-first ordering |
| `--version` | Print version |
| `-h, --help` | Show help |

---

## Notes

- Automatically ignores: `.git`, `node_modules`, `__pycache__`, `target`, `build`
- Color is auto-disabled when output is piped — no need to pass `--no-color` manually
- `[empty]` means the directory has no direct files (subdirectories may still exist)
- Cross-platform: Linux and macOS natively, Windows via `#ifdef` guards
- Single-pass traversal per directory — each folder is read exactly once

---

## Project Structure

```
neotree/
├── main.c      entry point — parse, init, walk, summarise
├── cli.c/h     argument parsing
├── fs.c/h      portable filesystem abstraction (POSIX + Win32)
├── utils.c/h   colors, matching, dynamic entry array
└── tree.c/h    traversal and rendering
```

---

## Roadmap

- [ ] Gitignore-aware filtering
- [ ] Glob pattern improvements (`**/*.c`, character classes)
- [ ] `--stats` flag for aggregate size info
- [ ] Windows native path separator polish

---

## License

MIT

---

## Author

Sanket Bharadwaj