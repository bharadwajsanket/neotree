# neotree

![C](https://img.shields.io/badge/language-C99-blue)
![version](https://img.shields.io/badge/version-v0.3.0-blue)
![status](https://img.shields.io/badge/status-active-success)
![platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)
![license](https://img.shields.io/badge/license-MIT-green)

**A fast, minimal tree CLI with filtering, sorting, ignore rules, and export support.**

Built for developers who need focused directory exploration — not just a static tree dump.

---

## Install

### Linux / macOS

```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

Pin a specific version:

```bash
VERSION=v0.3.0 curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
```

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
```

Pin a specific version:

```powershell
$env:VERSION="v0.3.0"; irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
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
| Recursive glob | `--pattern "**/*.c"` |
| Path-aware glob | `--pattern "src/**/*.h"` |
| Skip `node_modules`, `build`, etc. | `--ignore <name>` |
| Respect `.gitignore` automatically | _(no flag needed)_ |
| Show hidden dot-files | `--all` |
| Directories only | `--dirs-only` |
| File sizes | `--size` |
| Limit depth | `-L <n>` |
| Sort by name / size / modified | `--sort <key>` |
| Extension summary | `--ext-summary` |
| Export to plain text | `--export-txt file.txt` |
| Export to markdown | `--export-markdown file.md` |
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
├── README.md   (6 KB)
├── cli.c       (13 KB)
├── main.c      (4 KB)
└── tree.c      (15 KB)

0 directories, 6 files
```

```
$ neotree --dirs-only .

.
├── src/  (3 files)
└── include/  (2 files)

2 directories
```

> Note: `--dirs-only` omits the file count from the summary line.

```
$ neotree --pattern '**/*.c' .

.
├── cli.c
├── fs.c
├── main.c
├── tree.c
└── utils.c

0 directories, 5 files
```

```
$ neotree --pattern 'src/**/*.h' .

.
└── src/  (1 file)
    ├── internal/  (1 file)
    │   └── bar.h
    └── foo.h

2 directories, 2 files
```

> Note: `src/**/*.h` only matches files **under** `src/`. Use `**/*.h` to match headers everywhere.

```
$ neotree --ext-summary .

.
├── cli.c
├── cli.h
...

0 directories, 13 files

Extension summary:
  .c            5
  .h            4
  (no ext)      1
  .md           1
  .sh           1
```

```
$ neotree --export-markdown tree.md .
```

Writes a fenced markdown code block to `tree.md`. The export file itself is automatically excluded from the tree output.

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
neotree --pattern "**/*.c"            # .c files at any depth
neotree --pattern "src/**/*.h"        # .h files only under src/
neotree --sort size                   # sort by file size
neotree --sort modified               # sort by last modified
neotree --ext-summary                 # print extension counts
neotree --export-txt tree.txt         # export to plain text
neotree --export-markdown tree.md     # export to markdown
neotree --ignore dist                 # skip a directory
neotree --ignore dist --ignore .cache # multiple ignores
neotree --no-color | tee tree.txt     # pipe-safe plain text
neotree --no-dirs-first               # pure alphabetical order
```

> Always quote glob patterns: `--pattern "*.c"`, not `--pattern *.c`

---

## Options

| Flag | Description |
|---|---|
| `-L <depth>` | Limit display depth |
| `--all` | Show hidden files and directories (dot-entries) |
| `--dirs-only` | Show directories only; summary line omits file count |
| `--size` | Show file size in KB next to each file |
| `--pattern <glob>` | Show only files matching glob (`*.c`, `**/*.h`, `src/**/*.h`) |
| `--ignore <name>` | Ignore a file or directory by name (repeatable) |
| `--sort <key>` | Sort entries: `name` (default), `size`, `modified` |
| `--ext-summary` | Print extension counts after the tree |
| `--export-txt <file>` | Write plain-text tree to file (no ANSI codes) |
| `--export-markdown <file>` | Write fenced markdown tree to file |
| `--no-color` | Disable ANSI colors |
| `--no-dirs-first` | Disable directory-first ordering |
| `--version` | Print version |
| `-h, --help` | Show help |

---

## Glob Patterns

neotree supports two levels of glob matching:

| Pattern | What it matches |
|---|---|
| `*.c` | `.c` files in the current directory only |
| `**/*.c` | `.c` files at **any** depth |
| `src/**/*.h` | `.h` files **only under** `src/` |
| `foo/**/bar.py` | `bar.py` anywhere under `foo/` |

**Path-aware semantics:**  
`--pattern 'src/**/*.h'` matches `src/foo.h` and `src/a/b/c.h`  
but does **not** match `cli.h` or `include/test.h`.

Use `**/*.h` to match all headers regardless of location.

**Limitations:**
- No `?` or character-class (`[abc]`) wildcards
- Pattern separators must be `/` (even on Windows)
- Not full bash glob emulation

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

## Export

```bash
neotree --export-txt tree.txt         # plain text, no ANSI codes
neotree --export-markdown tree.md     # fenced code block
neotree --export-txt out.txt --export-markdown out.md  # both at once
```

- Export files are **automatically excluded** from the tree output.
- Markdown export wraps the entire tree in a ` ``` ` fenced code block.
- Export files contain no ANSI color codes regardless of terminal state.
- Export errors (bad path, permissions) are reported and exit cleanly.

---

## Sorting

```bash
neotree --sort name      # alphabetical (default)
neotree --sort size      # ascending file size (smallest first)
neotree --sort modified  # most recently modified first
```

Sorting applies within the directories-first grouping.  
Use `--no-dirs-first` to sort everything together without grouping.

---

## Extension Summary

```bash
neotree --ext-summary src/
```

Prints a count of file extensions after the tree, sorted by frequency:

```
Extension summary:
  .c            5
  .h            4
  .md           1
  (no ext)      1
```

Respects all filters (`--pattern`, `--ignore`, `--all`, `.gitignore`).

---

## Platform Support

| Platform | Status |
|---|---|
| Linux | ✅ Fully supported |
| macOS | ✅ Fully supported |
| Windows | ✅ Prebuilt `.exe` — install via PowerShell, no admin required |

**macOS note:** On macOS, system headers are under the Xcode SDK path, not `/usr/include`.  
If `/usr/include` is absent, neotree correctly reports it as missing.

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
- Export targets are automatically excluded from the generated tree

---

## Project Structure

```
neotree/
├── main.c      entry point — parse, init, walk, summarise, export
├── cli.c/h     argument parsing + .gitignore loading
├── fs.c/h      portable filesystem abstraction (POSIX + Win32)
├── utils.c/h   colors, matching, glob engine, dynamic array, ext-summary
└── tree.c/h    traversal and rendering
```

---

## Roadmap

- [x] `.gitignore`-aware filtering
- [x] `--all` hidden file toggle
- [x] `--dirs-only` mode
- [x] `--size` file size display
- [x] Filetype-based colors (cyan, yellow, green, magenta, purple, red)
- [x] `--sort name|size|modified`
- [x] `--ext-summary` extension breakdown
- [x] `--export-txt` / `--export-markdown`
- [x] Recursive glob (`**/*.c`, `src/**/*.h`) with path-aware matching
- [x] Prebuilt binaries via GitHub Actions
- [ ] `--stats` flag for aggregate size info
- [ ] Windows native path separator polish in patterns

---

## Contributing

This project is intentionally simple and hackable.

If something breaks — open an issue.  
If something feels missing — build it.  
If you can improve performance or UX — send a PR.

Good first contributions:
- edge cases in `.gitignore` parsing
- new flags (JSON output, etc.)
- performance improvements
- cross-platform fixes (especially Windows)

No strict rules — keep it clean and readable.

---

## License

MIT — [Sanket Bharadwaj](https://github.com/bharadwajsanket)
