# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.5.4] - 2026-06-09

### Added
- **Wildcard Expansion Suggestion**: Added a suggestion hint system that detects when the shell has prematurely expanded wildcards (e.g. `neotree --find *.c`) and prints an instructions hint recommending quoting patterns.
- **Gitignore Limit Warning**: Added warning to `stderr` when `.gitignore` entries are dropped due to reaching the maximum capacity limit of 64 rules.
- **Architecture-Aware Installer**: Updated `install.sh` to parse CPU architecture via `uname -m`, mapping and downloading the correct assets (`x86_64`, `arm64`, `armv7`). Adds detailed compilation instructions on unsupported platforms.
- **Architecture Build Targets**: Expanded GitHub Release CI workflow to build and upload separate target assets for Linux and macOS.

### Changed
- **Version Bump**: Bumped version to `1.5.4` globally.
- **Unified Statistics**: Display the `max depth` metric under `Stats:` even when `--dirs-only` is not specified.

### Fixed
- **Infinite Recursion on Path Truncation**: Modified `fs_join` and recursion walks to detect path lengths exceeding `4096` characters, skipping them gracefully instead of causing recursive stack overflows.
- **Depth Option Overflow**: Added range checks to `-L` parsing (restricting values between 1 and 10000) to prevent integer overflows.
- **Year 2038 Timestamp Overflow**: Changed internal `mtime` storage and native attributes to `long long` to prevent time truncation errors after 2038 on LLP64 and 32-bit platforms.
- **ANSI Leak in Exports**: Refactored rendering logic to prevent ANSI escape colors from leaking into generated markdown or text export files.

### Performance
- **Single-Pass Exports**: Refactored `tree_walk` to render plain-text layout to both `.txt` and `.md` export streams in a single walk pass, cutting directory I/O operations by 50%.

### Documentation
- Updated `README.md` and the manpage `man/neotree.1` with wildcard quoting advice, architecture support guides, and Windows ANSI path limitation details.

## [0.5.4] - 2026-05-18

### Fixed
- **Windows UTF-8 Mojibake**: Resolved console display glitches on Windows by using `SetConsoleOutputCP(CP_UTF8)` and `SetConsoleCP(CP_UTF8)` to correctly render box-drawing and Unicode/emoji characters.

## [0.4.0] - 2026-05-10

### Added
- **Multi-Root Support**: Added support for traversing and rendering multiple directory paths sequentially.
- **Isolated Find Mode**: Implemented flat filesystem searches (`--find` and `--find-dir`) that skip tree rendering and output raw paths matching glob/comma-separated queries.
- **Statistics Output**: Added `--stats` statistics output tracking file counts, size summaries, and extension frequency breakdowns.

### Changed
- **License Migration**: Transitioned repository license to GPLv3.

## [0.3.1] - 2026-04-20

### Added
- **Initial Release**: Initial public stable release of `neotree` containing core single-path directory tree rendering, color maps, depth limits (`-L`), text export (`--export-txt`), and Homebrew tap formula support.