# install.ps1 — neotree Windows installer
#
# Usage:
#   irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex
#
# Override version:
#   $env:VERSION = "v0.2.4"; irm ... | iex
#
# What it does:
#   1. Creates install directory under %LOCALAPPDATA%\Programs\neotree
#   2. Downloads prebuilt neotree-windows.exe from GitHub Releases
#   3. Renames it to neotree.exe
#   4. Adds the directory to the current user PATH (no admin required)
#   5. Verifies the install

#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ------------------------------------------------------------------ #
#  Config                                                              #
# ------------------------------------------------------------------ #

$RepoOwner  = "bharadwajsanket"
$RepoName   = "neotree"
$Version    = if ($env:VERSION) { $env:VERSION } else { "v0.3.0" }
$Asset      = "neotree-windows.exe"
$BinaryName = "neotree.exe"
$InstallDir = Join-Path $env:LOCALAPPDATA "Programs\neotree"
$DownloadUrl = "https://github.com/$RepoOwner/$RepoName/releases/download/$Version/$Asset"

# ------------------------------------------------------------------ #
#  Helpers                                                             #
# ------------------------------------------------------------------ #

function Write-Info    ($msg) { Write-Host "==> $msg" -ForegroundColor Cyan   }
function Write-Success ($msg) { Write-Host "  v $msg" -ForegroundColor Green  }
function Write-Warn    ($msg) { Write-Host "  ! $msg" -ForegroundColor Yellow }
function Write-Fail    ($msg) { Write-Host "error: $msg" -ForegroundColor Red; exit 1 }

# ------------------------------------------------------------------ #
#  Banner                                                              #
# ------------------------------------------------------------------ #

Write-Host ""
Write-Host "neotree installer" -ForegroundColor White
Write-Host "-----------------"
Write-Host ""

Write-Info "Version:     $Version"
Write-Info "Install dir: $InstallDir"

# ------------------------------------------------------------------ #
#  1. Create install directory                                         #
# ------------------------------------------------------------------ #

if (-not (Test-Path $InstallDir)) {
    Write-Info "Creating $InstallDir..."
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    Write-Success "Directory created"
} else {
    Write-Info "Directory already exists"
}

# ------------------------------------------------------------------ #
#  2. Download binary                                                  #
# ------------------------------------------------------------------ #

$TmpFile = Join-Path $InstallDir $Asset

Write-Info "Downloading $Asset $Version..."

try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $TmpFile -UseBasicParsing
} catch {
    Write-Fail "Download failed.
  URL: $DownloadUrl
  Check that $Version exists at:
  https://github.com/$RepoOwner/$RepoName/releases"
}

Write-Success "Downloaded $Asset"

# ------------------------------------------------------------------ #
#  3. Rename to neotree.exe                                            #
# ------------------------------------------------------------------ #

$BinaryPath = Join-Path $InstallDir $BinaryName

if (Test-Path $BinaryPath) {
    Write-Warn "Existing neotree.exe found, replacing it"
    Remove-Item $BinaryPath -Force
}

Move-Item -Path $TmpFile -Destination $BinaryPath
Write-Success "Installed to $BinaryPath"

# ------------------------------------------------------------------ #
#  4. Add install directory to user PATH                               #
# ------------------------------------------------------------------ #

$UserPath = [Environment]::GetEnvironmentVariable("PATH", "User")

if ($UserPath -split ";" -contains $InstallDir) {
    Write-Info "PATH already contains $InstallDir"
} else {
    Write-Info "Adding $InstallDir to user PATH..."
    $NewPath = "$UserPath;$InstallDir".TrimStart(";")
    [Environment]::SetEnvironmentVariable("PATH", $NewPath, "User")
    # Also update the current session so the binary is usable immediately
    $env:PATH = "$env:PATH;$InstallDir"
    Write-Success "PATH updated"
}

# ------------------------------------------------------------------ #
#  5. Verify                                                           #
# ------------------------------------------------------------------ #

try {
    $VersionOutput = & $BinaryPath --version
    Write-Host ""
    Write-Success "$VersionOutput installed successfully."
} catch {
    Write-Warn "neotree was installed but could not be verified."
    Write-Warn "Open a new terminal and run: neotree --version"
}

Write-Host ""
Write-Host "Run it:" -ForegroundColor White
Write-Host "  neotree"
Write-Host "  neotree --all"
Write-Host "  neotree --size"
Write-Host "  neotree -L 2 ."
Write-Host ""
Write-Host "To uninstall:" -ForegroundColor White
Write-Host "  Remove-Item '$BinaryPath'"
Write-Host "  # Then remove '$InstallDir' from your user PATH"
Write-Host ""
