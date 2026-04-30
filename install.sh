#!/usr/bin/env bash
# install.sh — neotree installer
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
#
# What it does:
#   1. Detects OS (Linux / macOS)
#   2. Checks for a C compiler (gcc preferred, clang fallback)
#   3. Checks for git
#   4. Clones the repo into a temp dir
#   5. Compiles with detected compiler
#   6. Installs binary to /usr/local/bin (uses sudo if needed)
#   7. Cleans up
#   8. Verifies the install with `neotree --version`

set -euo pipefail

# ------------------------------------------------------------------ #
#  Config                                                              #
# ------------------------------------------------------------------ #

REPO="https://github.com/bharadwajsanket/neotree.git"
BINARY="neotree"
INSTALL_DIR="/usr/local/bin"
SRCS="main.c tree.c fs.c cli.c utils.c"

# Pin to a specific release tag, or "main" for latest.
# To release a version: git tag v0.1.0 && git push --tags
# Then bump this to VERSION="v0.1.0"
VERSION="main"
TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t neotree)"

# ------------------------------------------------------------------ #
#  Helpers                                                             #
# ------------------------------------------------------------------ #

# tput-safe color: falls back to plain text if not a TTY / tput absent
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
    RED="$(tput setaf 1)"
    GRN="$(tput setaf 2)"
    YLW="$(tput setaf 3)"
    CYN="$(tput setaf 6)"
    BLD="$(tput bold)"
    RST="$(tput sgr0)"
else
    RED="" GRN="" YLW="" CYN="" BLD="" RST=""
fi

info()    { printf "${CYN}==>${RST} %s\n" "$*"; }
success() { printf "${GRN}  ✓${RST} %s\n" "$*"; }
warn()    { printf "${YLW}  !${RST} %s\n" "$*"; }
die()     { printf "${RED}${BLD}error:${RST} %s\n" "$*" >&2; exit 1; }

# Cleanup on exit (success or failure)
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

# ------------------------------------------------------------------ #
#  Banner                                                              #
# ------------------------------------------------------------------ #

printf "\n${BLD}neotree installer${RST}\n"
printf "─────────────────\n\n"

# ------------------------------------------------------------------ #
#  1. Detect OS                                                        #
# ------------------------------------------------------------------ #

OS="$(uname -s)"
case "$OS" in
    Linux*)   OS_NAME="Linux"  ;;
    Darwin*)  OS_NAME="macOS"  ;;
    *)        die "Unsupported OS: $OS. Only Linux and macOS are supported." ;;
esac
info "Detected OS: $OS_NAME"
info "Version: $VERSION"

# ------------------------------------------------------------------ #
#  2. Compiler detection (gcc → clang → fail)                         #
# ------------------------------------------------------------------ #

CC=""
if command -v gcc >/dev/null 2>&1; then
    CC="gcc"
    success "Found compiler: gcc ($(gcc --version | head -1))"
elif command -v clang >/dev/null 2>&1; then
    CC="clang"
    warn "gcc not found — falling back to clang ($(clang --version | head -1))"
else
    die "No C compiler found. Install gcc or clang and re-run:
       Linux:  sudo apt install gcc   (Debian/Ubuntu)
               sudo dnf install gcc   (Fedora)
       macOS:  xcode-select --install"
fi

# ------------------------------------------------------------------ #
#  3. git check                                                        #
# ------------------------------------------------------------------ #

if ! command -v git >/dev/null 2>&1; then
    die "git is required but not installed.
       Linux:  sudo apt install git
       macOS:  xcode-select --install"
fi
success "Found git ($(git --version))"

# ------------------------------------------------------------------ #
#  4. Clone                                                            #
# ------------------------------------------------------------------ #

info "Starting installation..."
info "Cloning neotree ($VERSION)..."
if ! git clone --depth=1 --branch "$VERSION" --quiet "$REPO" "$TMP_DIR" 2>/dev/null; then
    warn "Could not checkout '$VERSION', falling back to default branch"
    git clone --depth=1 --quiet "$REPO" "$TMP_DIR"
fi
success "Cloned repository"

# ------------------------------------------------------------------ #
#  5. Compile                                                          #
# ------------------------------------------------------------------ #

info "Building with $CC..."
cd "$TMP_DIR" || die "Failed to enter build directory: $TMP_DIR"

# word-split intentional: SRCS is a space-separated string
# shellcheck disable=SC2086
"$CC" -std=c99 -Wall -Wextra -O2 $SRCS -o "$BINARY" \
    || die "Build failed. Check the output above for compiler errors."

success "Build successful"

# ------------------------------------------------------------------ #
#  6. Install                                                          #
# ------------------------------------------------------------------ #

info "Installing to ${INSTALL_DIR}/${BINARY}..."

# Check if install dir is writable without sudo
if [ -w "$INSTALL_DIR" ]; then
    mv "$BINARY" "$INSTALL_DIR/$BINARY"
else
    # Need sudo — tell the user explicitly rather than silently escalating
    warn "$INSTALL_DIR is not writable. Requesting sudo for install step only."
    sudo mv "$BINARY" "$INSTALL_DIR/$BINARY"
fi

success "Installed to $INSTALL_DIR/$BINARY"

# ------------------------------------------------------------------ #
#  7. Verify                                                           #
# ------------------------------------------------------------------ #

# Make sure the binary is actually on PATH now
if ! command -v "$BINARY" >/dev/null 2>&1; then
    warn "neotree was installed to $INSTALL_DIR but it's not in your PATH."
    warn "Add this to your shell config:"
    warn "  export PATH=\"\$PATH:$INSTALL_DIR\""
    exit 0
fi

printf "\n"
success "$($BINARY --version) is ready."
printf "\n${BLD}Run it:${RST}\n"
printf "  neotree\n"
printf "  neotree -L 2 .\n"
printf "  neotree --pattern '*.c' src/\n"
printf "\n${BLD}To uninstall:${RST}\n"
printf "  sudo rm $INSTALL_DIR/$BINARY\n\n"