param(
  [switch]$IncludeGlobalCache
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Section([string]$Title) {
  Write-Host ""
  Write-Host "==== $Title ====" -ForegroundColor Cyan
}

Write-Section "Clean NuGet Cache and Restore Files"

# Get repo root
$scriptRoot = Split-Path -Parent $PSScriptRoot
$root = Split-Path -Parent $scriptRoot

# Paths to clean
$projectDir = Join-Path $root "src\desktop_host"
$objDir = Join-Path $root "out\obj\desktop_host"
$binDir = Join-Path $root "out\desktop_host"

Write-Host "Cleaning project build artifacts..." -ForegroundColor Yellow
if (Test-Path $objDir) {
  Write-Host "  Removing: $objDir"
  Remove-Item -Path $objDir -Recurse -Force -ErrorAction SilentlyContinue
}
if (Test-Path $binDir) {
  Write-Host "  Removing: $binDir"
  Remove-Item -Path $binDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Clean package lock files
Write-Host "Cleaning lock files..." -ForegroundColor Yellow
$lockFile = Join-Path $projectDir "packages.lock.json"
if (Test-Path $lockFile) {
  Write-Host "  Removing: $lockFile"
  Remove-Item -Path $lockFile -Force -ErrorAction SilentlyContinue
}

if ($IncludeGlobalCache) {
  Write-Host "Cleaning global NuGet cache (this may take a minute)..." -ForegroundColor Yellow
  $nugetCache = Join-Path $env:USERPROFILE ".nuget\packages"
  
  # Clean specific packages for the desktop host project
  $pkgsToDel = @(
    "microsoft.windows.appSdk",
    "microsoft.windows.cppwinrt",
    "microsoft.windowsappsdk"
  )
  
  foreach ($pkg in $pkgsToDel) {
    $pkgPath = Join-Path $nugetCache $pkg
    if (Test-Path $pkgPath) {
      Write-Host "  Removing: $pkgPath"
      Remove-Item -Path $pkgPath -Recurse -Force -ErrorAction SilentlyContinue
    }
  }
}

Write-Section "Clean Complete"
Write-Host "Next step: Run the build script with fresh NuGet restore" -ForegroundColor Green
Write-Host "  ./scripts/windows/build.ps1 -Target desktop_host" -ForegroundColor Gray
