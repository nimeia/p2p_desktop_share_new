param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [ValidateSet("x64")] [string]$Arch = "x64"
)

. (Join-Path $PSScriptRoot "common.ps1")

$root = Get-RepoRoot
$exe = Join-Path $root "desktop_host\LanScreenShareHostApp\bin\$Arch\$Config\LanScreenShareHostApp.exe"
if (-not (Test-Path $exe)) { Fail "Desktop host exe not found. Build first: scripts\windows\build.ps1 -Target desktop_host" }
Write-Section "Run desktop host app"
& $exe
