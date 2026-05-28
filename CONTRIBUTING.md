# Contributing to neotree

neotree is a small, focused UNIX utility. Contributions that improve correctness, portability, or robustness are welcome. Proposals that expand scope should be discussed in an issue first.

## Building and testing

```bash
make           # build
make test      # build and run the full test suite
make debug     # build with AddressSanitizer + UBSan
```

Tests live in `test.sh`. New behavior should be covered by a test case.

## Code style

- C99, no compiler extensions
- Follow the existing style exactly — indentation, brace placement, comment format
- Functions should do one thing and be short enough to read at a glance
- Comments explain *why*, not *what*
- No external dependencies, ever

## Portability

neotree builds on Linux, macOS, and Windows (MinGW). Platform-specific code lives entirely in `fs.c` behind `#ifdef _WIN32`. Any change touching platform behavior must be considered on all three targets.

## What belongs in neotree

neotree intentionally avoids:

- External dependencies of any kind
- Configuration files or plugin systems
- Interactive interfaces (TUI, ncurses, pagers)
- Fuzzy search or matching beyond the existing glob engine
- Daemon processes, async runtimes, or network access

If a feature conflicts with these constraints, it is out of scope. The goal is a utility that compiles to a single binary, starts instantly, and does exactly what it says.

## Pull requests

- Keep changes small and focused — one fix or improvement per PR
- `make test` must pass before submitting
- If you are unsure whether something fits, open an issue first

## Commit messages

Short, lowercase, imperative:

```
fix: find_walk now respects the ignore list
cleanup: remove stale comment in utils.c
ci: add per-PR build and test workflow
docs: fix --sort modified direction in README and manpage
```
