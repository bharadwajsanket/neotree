#!/usr/bin/env bash
# install.sh — neotree installer
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
#
# Override version:
#   VERSION=v0.3.1 curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash
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
MAN_INSTALL_DIR="/usr/local/share/man/man1"

# Pin to a specific release. Users may override:
#   VERSION=v1.5.4 curl -sSL ... | bash
VERSION="${VERSION:-v1.5.4}"

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
ARCH="$(uname -m)"
case "$OS" in
    Linux*)
        OS_NAME="Linux"
        case "$ARCH" in
            x86_64|amd64)   ASSET="neotree-linux-x86_64" ;;
            arm64|aarch64)  ASSET="neotree-linux-arm64"  ;;
            armv7*)         ASSET="neotree-linux-armv7"  ;;
            *)
                warn "Unsupported architecture: $ARCH"
                die "To install neotree on Linux $ARCH, please build from source:
       git clone https://github.com/${REPO_OWNER}/${REPO_NAME}
       cd ${REPO_NAME} && make && sudo make install"
                ;;
        esac
        ;;
    Darwin*)
        OS_NAME="macOS"
        case "$ARCH" in
            x86_64|amd64)   ASSET="neotree-macos-x86_64" ;;
            arm64|aarch64)  ASSET="neotree-macos-arm64"  ;;
            *)
                warn "Unsupported architecture: $ARCH"
                die "To install neotree on macOS $ARCH, please build from source:
       git clone https://github.com/${REPO_OWNER}/${REPO_NAME}
       cd ${REPO_NAME} && make && sudo make install"
                ;;
        esac
        ;;
    *)        die "Unsupported OS: $OS. Only Linux and macOS are supported.
       For Windows, download neotree.exe from:
       https://github.com/${REPO_OWNER}/${REPO_NAME}/releases" ;;
esac

info "Detected OS:   $OS_NAME"
info "Detected Arch: $ARCH"
info "Version:       $VERSION"

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
MAN_URL="https://raw.githubusercontent.com/${REPO_OWNER}/${REPO_NAME}/${VERSION}/man/neotree.1"

info "Downloading ${ASSET} ${VERSION}..."

if ! curl -fL "$DOWNLOAD_URL" -o "$TMP_DIR/$BINARY"; then
    die "Download failed.
       URL: $DOWNLOAD_URL
       Check that ${VERSION} exists at:
       https://github.com/${REPO_OWNER}/${REPO_NAME}/releases"
fi

success "Downloaded $ASSET"

info "Downloading manpage..."
if ! curl -fL "$MAN_URL" -o "$TMP_DIR/neotree.1"; then
    warn "Could not download manpage from $MAN_URL. Skipping manpage installation."
fi

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
    ln -sf "$BINARY" "$INSTALL_DIR/ntree"
else
    warn "$INSTALL_DIR is not writable. Requesting sudo for install step only."
    sudo mv "$TMP_DIR/$BINARY" "$INSTALL_DIR/$BINARY"
    sudo ln -sf "$BINARY" "$INSTALL_DIR/ntree"
fi

success "Installed to $INSTALL_DIR/$BINARY"

if [ -f "$TMP_DIR/neotree.1" ]; then
    info "Installing manpage to ${MAN_INSTALL_DIR}/..."
    if [ -w "$MAN_INSTALL_DIR" ] || [ -w "$(dirname "$MAN_INSTALL_DIR")" ]; then
        mkdir -p "$MAN_INSTALL_DIR"
        cp "$TMP_DIR/neotree.1" "$MAN_INSTALL_DIR/neotree.1"
        ln -sf neotree.1 "$MAN_INSTALL_DIR/ntree.1"
    else
        warn "$MAN_INSTALL_DIR is not writable. Requesting sudo for manpage install."
        sudo mkdir -p "$MAN_INSTALL_DIR"
        sudo cp "$TMP_DIR/neotree.1" "$MAN_INSTALL_DIR/neotree.1"
        sudo ln -sf neotree.1 "$MAN_INSTALL_DIR/ntree.1"
    fi
    success "Installed manpage to $MAN_INSTALL_DIR/neotree.1"
fi

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
printf "  sudo rm -f %s/%s %s/ntree\n" "$INSTALL_DIR" "$BINARY" "$INSTALL_DIR"
printf "  sudo rm -f %s/neotree.1 %s/ntree.1\n\n" "$MAN_INSTALL_DIR" "$MAN_INSTALL_DIR"