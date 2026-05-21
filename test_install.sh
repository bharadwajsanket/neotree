#!/usr/bin/env bash
# test_install.sh — end-to-end installer + runtime validation for neotree
#
# Usage:
#   bash test_install.sh
#
# Runs the install script, verifies the binary, and exercises core features.
# Requires curl and sudo (for uninstall step).

set -euo pipefail

VERSION="${VERSION:-v0.3.1}"
REPO="https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh"

RED="\033[31m"
GRN="\033[32m"
YLW="\033[33m"
CYN="\033[36m"
RST="\033[0m"

info() { printf "${CYN}==>${RST} %s\n" "$*"; }
pass() { printf "${GRN}PASS${RST} %s\n" "$*"; }
warn() { printf "${YLW}WARN${RST} %s\n" "$*"; }
fail() { printf "${RED}FAIL${RST} %s\n" "$*"; exit 1; }

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

printf "\n"
printf "============================================\n"
printf "NEOTREE INSTALLER TEST SUITE\n"
printf "============================================\n\n"

# ----------------------------------------------------------
# 1. Environment
# ----------------------------------------------------------

info "Environment"
uname -a
echo

# ----------------------------------------------------------
# 2. curl check
# ----------------------------------------------------------

info "Checking curl"
command -v curl >/dev/null || fail "curl not installed"
pass "curl found"

# ----------------------------------------------------------
# 3. Install test
# ----------------------------------------------------------

info "Running installer"
curl -sSL "$REPO" | bash || fail "installer failed"
pass "installer completed"

# ----------------------------------------------------------
# 4. PATH check
# ----------------------------------------------------------

info "Checking PATH"
hash -r
command -v neotree >/dev/null || fail "neotree not in PATH"
which neotree
pass "binary visible in PATH"

# ----------------------------------------------------------
# 5. Version check
# ----------------------------------------------------------

info "Checking version"
INSTALLED_VERSION="$(neotree --version)"
echo "$INSTALLED_VERSION"
echo "$INSTALLED_VERSION" | grep "$VERSION" >/dev/null \
    || fail "wrong version installed"
pass "correct version installed"

# ----------------------------------------------------------
# 6. Help screen
# ----------------------------------------------------------

info "Checking --help"
neotree --help >/dev/null || fail "--help failed"
pass "--help works"

# ----------------------------------------------------------
# 7. Basic traversal
# ----------------------------------------------------------

info "Basic traversal"
cd "$TMP_DIR"
mkdir -p project/src/internal
touch project/main.c
touch project/src/test.h
touch project/src/internal/core.h
touch project/README.md
neotree project
pass "basic traversal works"

# ----------------------------------------------------------
# 8. Recursive glob
# ----------------------------------------------------------

info "Recursive glob matching"
OUTPUT="$(neotree --pattern 'src/**/*.h' project)"
echo "$OUTPUT"
echo "$OUTPUT" | grep "core.h" >/dev/null \
    || fail "recursive glob failed"
echo "$OUTPUT" | grep "README.md" \
    && fail "glob incorrectly matched README.md"
pass "recursive glob works"

# ----------------------------------------------------------
# 9. Backslash normalization
# ----------------------------------------------------------

info "Backslash normalization"
OUTPUT="$(neotree --pattern 'src\**\*.h' project)"
echo "$OUTPUT" | grep "core.h" >/dev/null \
    || fail "backslash normalization failed"
pass "backslash normalization works"

# ----------------------------------------------------------
# 10. Hidden files
# ----------------------------------------------------------

info "Hidden files"
touch project/.secret
DEFAULT="$(neotree project)"
ALL="$(neotree --all project)"
echo "$DEFAULT" | grep ".secret" \
    && fail "hidden file visible by default"
echo "$ALL" | grep ".secret" >/dev/null \
    || fail "--all failed"
pass "hidden file logic works"

# ----------------------------------------------------------
# 11. --dirs-only
# ----------------------------------------------------------

info "--dirs-only"
OUTPUT="$(neotree --dirs-only project)"
echo "$OUTPUT"
echo "$OUTPUT" | grep "files" \
    && fail "dirs-only still shows file counts"
pass "--dirs-only summary correct"

# ----------------------------------------------------------
# 12. TXT export
# ----------------------------------------------------------

info "TXT export"
neotree --export-txt tree.txt project >/dev/null
[ -f tree.txt ] || fail "tree.txt missing"
grep "tree.txt" tree.txt \
    && fail "export self-inclusion bug"
pass "txt export works"

# ----------------------------------------------------------
# 13. Markdown export
# ----------------------------------------------------------

info "Markdown export"
neotree --export-markdown tree.md project >/dev/null
[ -f tree.md ] || fail "tree.md missing"
grep "tree.md" tree.md \
    && fail "markdown self-inclusion bug"
grep '```' tree.md >/dev/null \
    || fail "markdown code fence missing"
pass "markdown export works"

# ----------------------------------------------------------
# 14. Pipe safety
# ----------------------------------------------------------

info "Pipe safety"
neotree project | cat >/dev/null || fail "pipe failed"
pass "pipe-safe output works"

# ----------------------------------------------------------
# 15. Invalid args
# ----------------------------------------------------------

info "Error handling"
neotree --sort garbage >/dev/null 2>&1 \
    && fail "invalid sort should fail"
pass "invalid sort handled"

# ----------------------------------------------------------
# 16. Reinstall
# ----------------------------------------------------------

info "Reinstall flow"
curl -sSL "$REPO" | bash >/dev/null
pass "reinstall succeeded"

# ----------------------------------------------------------
# 17. Uninstall
# ----------------------------------------------------------

info "Uninstall"
sudo rm -f /usr/local/bin/neotree
command -v neotree >/dev/null 2>&1 \
    && fail "uninstall failed"
pass "uninstall works"

# ----------------------------------------------------------
# 18. Fresh reinstall
# ----------------------------------------------------------

info "Fresh reinstall"
curl -sSL "$REPO" | bash >/dev/null
command -v neotree >/dev/null || \
    fail "reinstall after uninstall failed"
pass "fresh reinstall works"

# ----------------------------------------------------------
# DONE
# ----------------------------------------------------------

printf "\n"
printf "============================================\n"
printf "${GRN}ALL TESTS PASSED${RST}\n"
printf "============================================\n\n"
