# neotree

![C](https://img.shields.io/badge/language-C99-blue)
![version](https://img.shields.io/badge/version-v0.2.4-blue)
![status](https://img.shields.io/badge/status-active-success)
![platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)
![license](https://img.shields.io/badge/license-MIT-green)

**A fast, minimal tree CLI with filtering, ignore rules, and per-directory insights.**

Built for developers who need focused directory exploration — not just a static tree dump.

---

## Install

### Linux / macOS

```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

Pin a specific version:

```bash
VERSION=v0.2.4 curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

Pin a specific version:

```powershell
$env:VERSION="v0.2.4"; irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

### Manual Build

```bash
git clone https://github.com/bharadwajsanket/neotree
cd neotree
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 \
    main.c tree.c fs.c cli.c utils.c \
    -o neotree
```

### Uninstall

```bash
# Linux / macOS
sudo rm /usr/local/bin/neotree

# Windows
Remove-Item "$env:LOCALAPPDATA\Programs\neotree\neotree.exe"
```

---

## What it does

The standard `tree` command shows you structure and stops there.  
`neotree` adds everything you actually need:

| Capability | Flag |
|---|---|
| Show only `.c` files | `--pattern "*.c"` |
| Skip `node_modules`, `build`, etc. | `--ignore <name>` |
| Respect `.gitignore` automatically | _(no flag needed)_ |
| Show hidden dot-files | `--all` |
| Directories only | `--dirs-only` |
| File sizes | `--size` |
| Limit depth | `-L <n>` |
| Per-directory file counts | _(always on)_ |
| Detect empty folders | _(always on)_ |

---

## Examples

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
$ neotree --dirs-only -L 2 .

.
├── src/  [empty]
└── include/  [empty]

2 directories, 0 files
```

---

## Usage

```bash
neotree                               # current directory
neotree src/                          # specific path
neotree --all                         # show hidden files
neotree --dirs-only                   # directories only
neotree --size                        # show file sizes in KB
neotree -L 2                          # limit to 2 levels
neotree --pattern "*.c"               # only .c files
neotree --ignore dist                 # skip a directory
neotree --ignore dist --ignore .cache # multiple ignores
neotree --no-color | tee tree.txt     # pipe-safe plain text
neotree --no-dirs-first               # pure alphabetical order
```

> Always quote patterns: `--pattern "*.c"`, not `--pattern *.c`

---

## Options

| Flag | Description |
|---|---|
| `-L <depth>` | Limit display depth |
| `--all` | Show hidden files and directories (dot-entries) |
| `--dirs-only` | Show directories only; files are counted but not printed |
| `--size` | Show file size in KB next to each file |
| `--pattern <glob>` | Show only files matching pattern (`*.c`, `*.py`) |
| `--ignore <name>` | Ignore a file or directory by name (repeatable) |
| `--no-color` | Disable ANSI colors |
| `--no-dirs-first` | Disable directory-first ordering |
| `--version` | Print version |
| `-h, --help` | Show help |

---

## .gitignore Support

If a `.gitignore` exists in the scanned root, neotree reads it automatically.

Rules applied (minimal — not the full gitignore spec):

- Empty lines and `#` comments are skipped
- Trailing `/` is stripped (matched by name regardless of type)
- Exact name matches: `build`, `.DS_Store`
- Suffix glob matches: `*.o`, `*.log`
- Duplicates suppressed

> `.gitignore` rules apply even with `--all`. The `--all` flag only controls the hidden-file filter, not the ignore list.

---

## Platform Support

| Platform | Status |
|---|---|
| Linux | ✅ Fully supported |
| macOS | ✅ Fully supported |
| Windows | ✅ Prebuilt `.exe` — install via PowerShell, no admin required |

Prebuilt binaries for every release:
**<https://github.com/bharadwajsanket/neotree/releases>**

| Platform | Binary |
|---|---|
| Linux (x86_64) | `neotree-linux` |
| macOS | `neotree-macos` |
| Windows | `neotree-windows.exe` |

---

## Notes

- Auto-ignores: `.git`, `node_modules`, `__pycache__`, `target`, `build`
- Color is auto-disabled when piped — no need for `--no-color`
- `[empty]` = directory has no direct visible files (subdirs may still exist)
- Symlinks are listed but never followed (no infinite recursion)
- Single-pass traversal — each directory is read exactly once

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
- [x] Prebuilt binaries via GitHub Actions
- [ ] Glob pattern improvements (`**/*.c`, character classes)
- [ ] `--stats` flag for aggregate size info
- [ ] Windows native path separator polish

---

## License

MIT — [Sanket Bharadwaj](https://github.com/bharadwajsanket)
