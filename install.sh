#!/usr/bin/env bash
# install.sh — neotree installer
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
#
# Override version:
#   VERSION=v0.2.3 curl -sSL ... | bash
#
# What it does:
#   1. Detects OS (Linux / macOS)
#   2. Downloads the prebuilt binary for VERSION from GitHub Releases
#   3. Installs binary to /usr/local/bin (uses sudo if needed)
#   4. Verifies the install with `neotree --version`

set -euo pipefail

# ------------------------------------------------------------------ #
#  Config                                                              #
# ------------------------------------------------------------------ #

REPO_OWNER="bharadwajsanket"
REPO_NAME="neotree"
BINARY="neotree"
INSTALL_DIR="/usr/local/bin"

# Pin to a specific release. Users may override:
#   VERSION=v0.2.0 curl -sSL ... | bash
VERSION="${VERSION:-v0.2.3}"

# TMP_DIR is intentionally empty until mktemp runs so that cleanup()
# is safe to call even if the script fails before mktemp.
TMP_DIR=""

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

# Cleanup on exit (success or failure).
# Guard on -n so that an early failure before mktemp doesn't rm "/".
cleanup() {
    [ -n "$TMP_DIR" ] && rm -rf "$TMP_DIR"
}
trap cleanup EXIT

# ------------------------------------------------------------------ #
#  Banner                                                              #
# ------------------------------------------------------------------ #

printf "\n${BLD}neotree installer${RST}\n"
printf "─────────────────\n\n"

# ------------------------------------------------------------------ #
#  1. Detect OS → select binary name                                   #
# ------------------------------------------------------------------ #

OS="$(uname -s)"
case "$OS" in
    Linux*)   OS_NAME="Linux";  ASSET="neotree-linux"  ;;
    Darwin*)  OS_NAME="macOS";  ASSET="neotree-macos"  ;;
    *)        die "Unsupported OS: $OS. Only Linux and macOS are supported.
       For Windows, download neotree.exe from:
       https://github.com/${REPO_OWNER}/${REPO_NAME}/releases" ;;
esac

info "Detected OS: $OS_NAME"
info "Version:     $VERSION"

# ------------------------------------------------------------------ #
#  2. curl check                                                       #
# ------------------------------------------------------------------ #

if ! command -v curl >/dev/null 2>&1; then
    die "curl is required but not installed.
       Linux:  sudo apt install curl
       macOS:  curl ships with macOS; run: xcode-select --install"
fi

# ------------------------------------------------------------------ #
#  3. Download prebuilt binary                                         #
# ------------------------------------------------------------------ #

TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t neotree)"

DOWNLOAD_URL="https://github.com/${REPO_OWNER}/${REPO_NAME}/releases/download/${VERSION}/${ASSET}"

info "Downloading ${ASSET} ${VERSION}..."

if ! curl -fL "$DOWNLOAD_URL" -o "$TMP_DIR/$BINARY"; then
    die "Download failed.
       URL: $DOWNLOAD_URL
       Check that ${VERSION} exists at:
       https://github.com/${REPO_OWNER}/${REPO_NAME}/releases"
fi

success "Downloaded $ASSET"

# ------------------------------------------------------------------ #
#  4. Install                                                          #
# ------------------------------------------------------------------ #

chmod +x "$TMP_DIR/$BINARY"
mkdir -p "$INSTALL_DIR"

# Warn if an existing installation will be replaced
if command -v "$BINARY" >/dev/null 2>&1; then
    warn "Existing neotree found, replacing it"
fi

info "Installing to ${INSTALL_DIR}/${BINARY}..."

if [ -w "$INSTALL_DIR" ]; then
    mv "$TMP_DIR/$BINARY" "$INSTALL_DIR/$BINARY"
else
    warn "$INSTALL_DIR is not writable. Requesting sudo for install step only."
    sudo mv "$TMP_DIR/$BINARY" "$INSTALL_DIR/$BINARY"
fi

success "Installed to $INSTALL_DIR/$BINARY"

# ------------------------------------------------------------------ #
#  5. Verify                                                           #
# ------------------------------------------------------------------ #

if ! command -v "$BINARY" >/dev/null 2>&1; then
    warn "neotree was installed to $INSTALL_DIR but it's not in your PATH."
    warn "Add this to your shell config:"
    warn "  export PATH=\"\$PATH:$INSTALL_DIR\""
    exit 0
fi

printf "\n"
success "$("$BINARY" --version) installed successfully."
printf "\n${BLD}Run it:${RST}\n"
printf "  neotree\n"
printf "  neotree --all\n"
printf "  neotree --size\n"
printf "  neotree -L 2 .\n"
printf "  neotree --pattern '*.c' src/\n"
printf "\n${BLD}To uninstall:${RST}\n"
printf "  sudo rm %s/%s\n\n" "$INSTALL_DIR" "$BINARY"