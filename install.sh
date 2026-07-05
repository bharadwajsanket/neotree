#!/usr/bin/env bash
# install.sh — neotree modern installer
# Tagline: Traversal by Instinct.
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.sh | bash

set -euo pipefail

# ------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------
REPO_OWNER="bharadwajsanket"
REPO_NAME="neotree"
BINARY="neotree"
VERSION="v2.5.4"
RAW_VERSION="2.5.4"

# ------------------------------------------------------------------
# CLI Arguments Parser
# ------------------------------------------------------------------
FORCE_INSTALL=false
UNINSTALL=false

show_help() {
    cat << EOF
neotree Installer $VERSION
Traversal by Instinct.

Usage:
  install.sh [OPTIONS]

Options:
  -f, --force      Force reinstall of the same or older version
  -u, --uninstall  Uninstall neotree and ntree from the system
  -h, --help       Show this help message and exit
  -v, --version    Show installer version and exit
EOF
    exit 0
}

show_version() {
    echo "neotree installer version $VERSION"
    exit 0
}

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        -f|--force)
            FORCE_INSTALL=true
            shift
            ;;
        -u|--uninstall)
            UNINSTALL=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        -v|--version)
            show_version
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            ;;
    esac
done

# ------------------------------------------------------------------
# Formatting Helper Functions
# ------------------------------------------------------------------
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

print_header() {
    printf "${BLD}============================================================${RST}\n"
    printf "${BLD}neotree Installer %s${RST}\n" "$VERSION"
    printf "Traversal by Instinct.\n"
    printf "${BLD}============================================================${RST}\n\n"
}

print_footer() {
    printf "${BLD}============================================================${RST}\n"
    printf "${GRN}${BLD}Installation Complete${RST}\n"
    printf "${BLD}============================================================${RST}\n\n"
    printf "Installed:\n"
    printf "    ntree %s\n\n" "$VERSION"
    printf "Traversal by Instinct.\n\n"
    printf "Run:\n\n"
    printf "    ntree --help\n\n"
    printf "${BLD}============================================================${RST}\n"
}

step_start() {
    printf "[•] %s...\n" "$1"
}

step_ok() {
    printf "${GRN}[✓]${RST} %s\n\n" "$*"
}

step_warn() {
    printf "${YLW}[!]${RST} %s\n\n" "$*"
}

die() {
    printf "${RED}error:${RST} %s\n" "$*" >&2
    exit 1
}

# ------------------------------------------------------------------
# Cleanup Handler
# ------------------------------------------------------------------
TMP_DIR=""
cleanup() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT

# ------------------------------------------------------------------
# Uninstall Mode Execution
# ------------------------------------------------------------------
if [ "$UNINSTALL" = true ]; then
    print_header
    step_start "Locating installation directories"
    
    # Check permissions & locations
    INSTALL_DIR="/usr/local/bin"
    MAN_INSTALL_DIR="/usr/local/share/man/man1"
    
    if [ ! -f "$INSTALL_DIR/neotree" ] && [ ! -f "$HOME/.local/bin/neotree" ]; then
        die "neotree is not installed in the default paths."
    fi
    
    if [ -f "$HOME/.local/bin/neotree" ]; then
        INSTALL_DIR="$HOME/.local/bin"
        MAN_INSTALL_DIR="$HOME/.local/share/man/man1"
    fi
    
    step_start "Removing binaries and links"
    USE_SUDO=false
    if [ ! -w "$INSTALL_DIR" ]; then
        USE_SUDO=true
    fi
    
    if [ "$USE_SUDO" = true ]; then
        sudo rm -f "$INSTALL_DIR/neotree" "$INSTALL_DIR/ntree"
        sudo rm -f "$MAN_INSTALL_DIR/neotree.1" "$MAN_INSTALL_DIR/ntree.1"
    else
        rm -f "$INSTALL_DIR/neotree" "$INSTALL_DIR/ntree"
        rm -f "$MAN_INSTALL_DIR/neotree.1" "$MAN_INSTALL_DIR/ntree.1"
    fi
    
    step_ok "Uninstallation complete."
    exit 0
fi

# ------------------------------------------------------------------
# Main Installer Logic
# ------------------------------------------------------------------
print_header

# 1. OS Detection
step_start "Detecting operating system"
OS="$(uname -s)"
OS_NAME=""
OS_LOWER=""
case "$OS" in
    Darwin*)
        OS_NAME="macOS"
        OS_LOWER="macos"
        ;;
    Linux*)
        OS_NAME="Linux"
        OS_LOWER="linux"
        ;;
    *)
        die "Unsupported OS: $OS. neotree only supports macOS and Linux."
        ;;
esac
step_ok "$OS_NAME"

# 2. Architecture Detection
step_start "Detecting architecture"
ARCH="$(uname -m)"
ARCH_NAME=""
ARCH_LOWER=""
case "$ARCH" in
    x86_64|amd64)
        ARCH_NAME="64-bit Intel/AMD (x86_64)"
        ARCH_LOWER="x86_64"
        ;;
    arm64|aarch64)
        ARCH_NAME="Apple Silicon / ARM64 (arm64)"
        ARCH_LOWER="arm64"
        ;;
    *)
        die "Unsupported CPU architecture: $ARCH"
        ;;
esac
step_ok "$ARCH_NAME"

# 3. Check Existing Installation
step_start "Checking existing installation"
EXISTING_PATH="$(command -v ntree || command -v neotree || true)"
INSTALLED_VER=""
if [ -n "$EXISTING_PATH" ] && [ -x "$EXISTING_PATH" ]; then
    INSTALLED_VER="$("$EXISTING_PATH" --version 2>/dev/null | awk '{print $2}' || true)"
fi

if [ -n "$INSTALLED_VER" ]; then
    step_ok "ntree $INSTALLED_VER detected"
    
    if [ "$INSTALLED_VER" = "$RAW_VERSION" ]; then
        if [ "$FORCE_INSTALL" = false ]; then
            step_warn "neotree version $VERSION is already installed. Use --force to reinstall."
            exit 0
        else
            step_start "Reinstalling version $VERSION"
        fi
    else
        step_start "Upgrade available: $INSTALLED_VER → $RAW_VERSION"
    fi
else
    step_ok "No existing installation found (fresh install)"
fi

# 4. Check for curl
if ! command -v curl >/dev/null 2>&1; then
    die "curl is required to download neotree. Please install curl."
fi

# 5. Determine Installation Directories based on permissions
INSTALL_DIR="/usr/local/bin"
MAN_INSTALL_DIR="/usr/local/share/man/man1"
USE_SUDO=false

# If /usr/local/bin is writable, or if we are root, or if sudo is available
if [ -w "$INSTALL_DIR" ]; then
    # All good
    :
elif command -v sudo >/dev/null 2>&1; then
    # Sudo available, we will request permission later when installing
    USE_SUDO=true
else
    # Fallback to local user space
    INSTALL_DIR="$HOME/.local/bin"
    MAN_INSTALL_DIR="$HOME/.local/share/man/man1"
    mkdir -p "$INSTALL_DIR"
    mkdir -p "$MAN_INSTALL_DIR"
fi

# 6. Download release archive
step_start "Downloading release"
ASSET="neotree-${VERSION}-${OS_LOWER}-${ARCH_LOWER}.tar.gz"
DOWNLOAD_URL="https://github.com/${REPO_OWNER}/${REPO_NAME}/releases/download/${VERSION}/${ASSET}"

TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t neotree)"

if ! curl -sSLf "$DOWNLOAD_URL" -o "$TMP_DIR/archive.tar.gz"; then
    die "Download failed. URL: $DOWNLOAD_URL. Make sure $VERSION exists on GitHub Releases."
fi

# Animate progress bar (satisfying terminal effect)
printf "    "
for i in {1..26}; do
    printf "█"
    sleep 0.005
done
printf " 100%%\n\n"

# 7. Extract archive
if ! tar -xzf "$TMP_DIR/archive.tar.gz" -C "$TMP_DIR"; then
    die "Extraction failed."
fi

# 8. Install binary and symlinks
step_start "Installing binary"
chmod +x "$TMP_DIR/neotree"

if [ "$USE_SUDO" = true ]; then
    sudo mkdir -p "$INSTALL_DIR"
    sudo mv -f "$TMP_DIR/neotree" "$INSTALL_DIR/neotree"
    sudo ln -sf neotree "$INSTALL_DIR/ntree"
else
    mkdir -p "$INSTALL_DIR"
    mv -f "$TMP_DIR/neotree" "$INSTALL_DIR/neotree"
    ln -sf neotree "$INSTALL_DIR/ntree"
fi
step_ok "Installed neotree and ntree alias to $INSTALL_DIR"

# 9. Install man page and symlinks
if [ -f "$TMP_DIR/neotree.1" ]; then
    step_start "Installing man page"
    if [ "$USE_SUDO" = true ]; then
        sudo mkdir -p "$MAN_INSTALL_DIR"
        sudo cp -f "$TMP_DIR/neotree.1" "$MAN_INSTALL_DIR/neotree.1"
        sudo ln -sf neotree.1 "$MAN_INSTALL_DIR/ntree.1"
    else
        mkdir -p "$MAN_INSTALL_DIR"
        cp -f "$TMP_DIR/neotree.1" "$MAN_INSTALL_DIR/neotree.1"
        ln -sf neotree.1 "$MAN_INSTALL_DIR/ntree.1"
    fi
    step_ok "Installed man pages to $MAN_INSTALL_DIR"
fi

# 10. Update/Validate PATH
step_start "Validating PATH configuration"
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    printf "${YLW}[!] Warning:${RST} $INSTALL_DIR is not in your PATH.\n"
    printf "    To run neotree commands directly, add it to your shell config file:\n"
    printf "    (e.g., ~/.bashrc, ~/.zshrc, or ~/.bash_profile)\n\n"
    printf "        export PATH=\"\$PATH:$INSTALL_DIR\"\n\n"
else
    step_ok "PATH is correctly configured"
fi

print_footer