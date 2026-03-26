param(
  [ValidateSet("user", "machine")]
  [string]$Scope = "user",
  [string]$PackageRoot = "",
  [switch]$CreateDesktopShortcut
)

$scriptDir = $PSScriptRoot
$helperDir = Join-Path $scriptDir "scripts\windows"
. (Join-Path $helperDir "package_common.ps1")

if (-not $PackageRoot) { $PackageRoot = $scriptDir }
$PackageRoot = (Resolve-Path $PackageRoot).Path
$manifestPath = Join-Path $PackageRoot "package_manifest.json"
if (-not (Test-Path -LiteralPath $manifestPath)) {
  Fail "package_manifest.json not found under $PackageRoot"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$version = [string]$manifest.version
$installDir = Get-InstallRoot -Scope $Scope

Write-Section "Install ViewMesh"
Write-Host "PackageRoot: $PackageRoot"
Write-Host "InstallDir:  $installDir"
Write-Host "Scope:       $Scope"

New-Item -ItemType Directory -Force -Path $installDir | Out-Null

Get-ChildItem -LiteralPath $PackageRoot -Force | ForEach-Object {
  if ($_.Name -eq ".git") { return }
  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $installDir $_.Name) -Recurse -Force
}

$exe = Join-Path $installDir "ViewMesh.exe"
if (-not (Test-Path -LiteralPath $exe)) {
  Fail "Installed payload is missing ViewMesh.exe"
}

$startMenuDir = if ($Scope -eq "machine") {
  Join-Path $env:ProgramData "Microsoft\Windows\Start Menu\Programs"
} else {
  Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
}
$startMenuShortcut = Join-Path $startMenuDir ($global:LanProductName + ".lnk")
New-Shortcut -ShortcutPath $startMenuShortcut -TargetPath $exe -Description "ViewMesh desktop host"
if ($CreateDesktopShortcut) {
  $desktop = [Environment]::GetFolderPath("Desktop")
  New-Shortcut -ShortcutPath (Join-Path $desktop ($global:LanProductName + ".lnk")) -TargetPath $exe -Description "ViewMesh desktop host"
}

Write-UninstallEntry -Scope $Scope -InstallDir $installDir -Version $version
Write-Host "Installed $global:LanProductName $version" -ForegroundColor Green
