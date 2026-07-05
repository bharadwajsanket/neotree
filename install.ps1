# install.ps1 — neotree Windows installer
# Tagline: Traversal by Instinct.
#
# Usage:
#   irm https://raw.githubusercontent.com/bharadwajsanket/neotree/main/install.ps1 | iex

[CmdletBinding()]
param(
    [switch]$Force,
    [switch]$Uninstall,
    [switch]$Help,
    [switch]$Version
)

#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------
$RepoOwner   = "bharadwajsanket"
$RepoName    = "neotree"
$InstallerVer = "v2.5.4"
$RawVersion   = "2.5.4"
$InstallDir  = Join-Path $env:LOCALAPPDATA "Programs\neotree"

# ------------------------------------------------------------------
# Formatting Helper Functions
# ------------------------------------------------------------------
function Write-Header {
    Write-Host "============================================================" -ForegroundColor Gray
    Write-Host "neotree Installer $InstallerVer" -ForegroundColor White
    Write-Host "Traversal by Instinct."
    Write-Host "============================================================" -ForegroundColor Gray
    Write-Host ""
}

function Write-Footer {
    Write-Host "============================================================" -ForegroundColor Gray
    Write-Host "Installation Complete" -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Installed:"
    Write-Host "    ntree $InstallerVer" -ForegroundColor White
    Write-Host ""
    Write-Host "Traversal by Instinct."
    Write-Host ""
    Write-Host "Run:"
    Write-Host "    ntree --help" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Gray
}

function Write-StepStart ($msg) {
    Write-Host "[•] $msg..."
}

function Write-StepOk ($msg) {
    Write-Host "  [✓] $msg`n" -ForegroundColor Green
}

function Write-StepWarn ($msg) {
    Write-Host "  [!] $msg`n" -ForegroundColor Yellow
}

function Write-StepFail ($msg) {
    Write-Host "error: $msg" -ForegroundColor Red
    exit 1
}

# ------------------------------------------------------------------
# CLI Arguments Check
# ------------------------------------------------------------------
if ($Help) {
    Write-Host "neotree Installer $InstallerVer"
    Write-Host "Traversal by Instinct."
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  install.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Force      Force reinstall of the same version"
    Write-Host "  -Uninstall  Uninstall neotree and ntree from the system"
    Write-Host "  -Help       Show this help message and exit"
    Write-Host "  -Version    Show installer version and exit"
    exit 0
}

if ($Version) {
    Write-Host "neotree installer version $InstallerVer"
    exit 0
}

# ------------------------------------------------------------------
# Uninstall Mode Execution
# ------------------------------------------------------------------
if ($Uninstall -or $env:UNINSTALL) {
    Write-Header
    Write-StepStart "Locating installation directory"
    if (Test-Path $InstallDir) {
        Write-StepStart "Removing binary files and folders"
        try {
            Remove-Item $InstallDir -Recurse -Force
            Write-StepOk "Removed neotree binaries"
        } catch {
            Write-StepFail "Could not remove directory: $_"
        }
    } else {
        Write-StepWarn "neotree is not installed in the default directory ($InstallDir)"
    }
    
    # Check User Path
    Write-StepStart "Checking User PATH configuration"
    $UserPath = [Environment]::GetEnvironmentVariable("PATH", "User")
    if ($UserPath -split ";" -contains $InstallDir) {
        Write-StepStart "Removing $InstallDir from User PATH"
        $CleanPaths = ($UserPath -split ";" | Where-Object { $_ -ne $InstallDir }) -join ";"
        [Environment]::SetEnvironmentVariable("PATH", $CleanPaths, "User")
        Write-StepOk "Removed $InstallDir from User PATH"
    } else {
        Write-StepOk "PATH is already clean"
    }
    exit 0
}

# ------------------------------------------------------------------
# Main Installer Logic
# ------------------------------------------------------------------
Write-Header

# 1. OS Verification
Write-StepStart "Detecting operating system"
Write-StepOk "Windows (PowerShell $($PSVersionTable.PSVersion))"

# 2. Architecture Detection
Write-StepStart "Detecting architecture"
$Arch = $env:PROCESSOR_ARCHITECTURE
$ArchSuffix = ""

# Map native engine architectures
if ($Arch -eq "ARM64") {
    $ArchSuffix = "arm64"
    Write-StepOk "Windows on ARM (arm64)"
} elseif ($Arch -eq "AMD64" -or $Arch -eq "IA64") {
    $ArchSuffix = "x64"
    Write-StepOk "64-bit Windows (x64)"
} else {
    Write-StepFail "Unsupported architecture: $Arch"
}

# 3. Check Existing Installation
Write-StepStart "Checking existing installation"
$ExistingPath = Get-Command ntree -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue
$InstalledVer = ""

if ($ExistingPath -and (Test-Path $ExistingPath)) {
    try {
        $InstalledVer = (& $ExistingPath --version 2>$null).Split(" ")[1]
    } catch {}
}

if ($InstalledVer) {
    Write-StepOk "ntree $InstalledVer detected"
    
    if ($InstalledVer -eq $RawVersion) {
        if (-not $Force) {
            Write-StepWarn "neotree version $InstallerVer is already installed. Use -Force to reinstall."
            exit 0
        } else {
            Write-StepStart "Reinstalling version $InstallerVer"
        }
    } else {
        Write-StepStart "Upgrade available: $InstalledVer -> $RawVersion"
    }
} else {
    Write-StepOk "No existing installation found (fresh install)"
}

# 4. Create install directory
Write-StepStart "Creating installation directory"
if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}
Write-StepOk "Directory configured at $InstallDir"

# 5. Download Release Asset Archive
$Asset = "neotree-$InstallerVer-windows-$ArchSuffix.zip"
$DownloadUrl = "https://github.com/$RepoOwner/$RepoName/releases/download/$InstallerVer/$Asset"
$TmpZip = Join-Path $InstallDir $Asset

Write-StepStart "Downloading release package"
try {
    # Ensure TLS 1.2
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $TmpZip -UseBasicParsing
} catch {
    Write-StepFail "Download failed. URL: $DownloadUrl. Make sure version exists on GitHub."
}

# Animate progress bar (satisfying terminal effect)
Write-Host -NoNewline "    "
for ($i = 1; $i -le 26; $i++) {
    Write-Host -NoNewline "█" -ForegroundColor Green
    Start-Sleep -Milliseconds 5
}
Write-Host " 100%`n"

# 6. Extract Zip Archive
Write-StepStart "Extracting package contents"
try {
    # Expand-Archive is built-in in PS 5.0+
    Expand-Archive -Path $TmpZip -DestinationPath $InstallDir -Force
    Remove-Item $TmpZip -Force
} catch {
    Write-StepFail "Extraction failed: $_"
}
Write-StepOk "Extracted to $InstallDir"

# 7. Create Symlink / Alias executable copy
Write-StepStart "Creating ntree binary alias"
$BinaryPath = Join-Path $InstallDir "neotree.exe"
$AliasPath  = Join-Path $InstallDir "ntree.exe"

if (-not (Test-Path $BinaryPath)) {
    Write-StepFail "neotree.exe not found in extracted archive."
}

if (Test-Path $AliasPath) {
    Remove-Item $AliasPath -Force
}
Copy-Item -Path $BinaryPath -Destination $AliasPath
Write-StepOk "Created alias executable"

# 8. Add Install Directory to user PATH if not present
Write-StepStart "Validating PATH configuration"
$UserPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($UserPath -split ";" -contains $InstallDir) {
    Write-StepOk "PATH is already configured"
} else {
    Write-StepStart "Adding $InstallDir to User PATH"
    $NewPath = "$UserPath;$InstallDir".TrimStart(";")
    [Environment]::SetEnvironmentVariable("PATH", $NewPath, "User")
    # Also update current session env variable
    $env:PATH = "$env:PATH;$InstallDir"
    Write-StepOk "PATH successfully updated"
}

Write-Footer
