param(
  [ValidateSet("user", "machine")]
  [string]$Scope = "user",
  [switch]$RemoveUserData
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$helperDir = Join-Path $scriptDir "scripts\windows"
. (Join-Path $helperDir "package_common.ps1")

$installDir = Get-InstallRoot -Scope $Scope
Write-Section "Uninstall LAN Screen Share"
Write-Host "InstallDir: $installDir"

$startMenuDir = if ($Scope -eq "machine") {
  Join-Path $env:ProgramData "Microsoft\Windows\Start Menu\Programs"
} else {
  Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
}
Remove-Shortcut (Join-Path $startMenuDir ($global:LanProductName + ".lnk"))
$desktop = [Environment]::GetFolderPath("Desktop")
Remove-Shortcut (Join-Path $desktop ($global:LanProductName + ".lnk"))

Remove-UninstallEntry -Scope $Scope
if (Test-Path -LiteralPath $installDir) {
  Remove-Item -LiteralPath $installDir -Recurse -Force -ErrorAction SilentlyContinue
}
if ($RemoveUserData) {
  $userData = Join-Path $env:LOCALAPPDATA "LanScreenShareHostApp"
  if (Test-Path -LiteralPath $userData) {
    Remove-Item -LiteralPath $userData -Recurse -Force -ErrorAction SilentlyContinue
  }
}
Write-Host "Removed $global:LanProductName" -ForegroundColor Green
