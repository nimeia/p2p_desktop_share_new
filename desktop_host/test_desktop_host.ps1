# Test script for LanScreenShareHostApp
$appPath = "D:\chatgpt-dev\lan_28\desktop_host\LanScreenShareHostApp\bin\x64\Debug\LanScreenShareHostApp.exe"

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
