# Test script for LanScreenShareHostApp
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$appPath = Join-Path $root "out\desktop_host\x64\Debug\LanScreenShareHostApp.exe"

if (-not (Test-Path $appPath)) {
    Write-Error "Executable not found: $appPath"
    exit 1
}

Write-Host "Launching $appPath..."
Write-Host "Window should appear shortly. Press Ctrl+C to exit."
Write-Host ""

# Start the application and wait for it
& $appPath

Write-Host "Application closed."
