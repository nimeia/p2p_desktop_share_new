param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [ValidateSet("x64")] [string]$Arch = "x64"
)

. (Join-Path $PSScriptRoot "common.ps1")

$root = Get-RepoRoot
$appPath = Get-DesktopHostExePath $root $Arch $Config
Assert-PathExists $appPath "desktop host executable"

Write-Section "Launch desktop host app"
Write-Host "Executable: $appPath"
Write-Host "Close the app window or press Ctrl+C in this console to stop."

& $appPath

Write-Host "Application closed."
