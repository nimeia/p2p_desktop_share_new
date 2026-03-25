param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Release",
  [ValidateSet("x64")]
  [string]$Arch = "x64",
  [string]$Version = "",
  [switch]$SkipBuild,
  [string]$OutputRoot = ""
)

. (Join-Path $PSScriptRoot "common.ps1")
. (Join-Path $PSScriptRoot "package_common.ps1")

$repoRoot = Get-RepoRoot
if (-not $OutputRoot) { $OutputRoot = Join-Path $repoRoot "out\package\windows" }
$desktopDir = Get-DesktopHostOutputDir $repoRoot $Arch $Config
$serverDir = Get-ServerOutputDir $repoRoot $Config
$versionResolved = Get-PackageVersion -RepoRoot $repoRoot -Version $Version
$packageName = "LanScreenShareHost_{0}_win-{1}" -f $versionResolved, $Arch
$stageDir = Join-Path $OutputRoot $packageName
$zipPath = Join-Path $OutputRoot ($packageName + ".zip")

if (-not $SkipBuild) {
  Write-Section "Build desktop + server"
  & (Join-Path $repoRoot "scripts\build.ps1") -Config $Config -Target all
  if ($LASTEXITCODE -ne 0) { Fail "Build failed while preparing package." }
}

Write-Section "Validate package payload"
$required = @(
  (Join-Path $desktopDir "LanScreenShareHostApp.exe"),
  (Join-Path $desktopDir "lan_screenshare_server.exe"),
  (Join-Path $desktopDir "www"),
  (Join-Path $desktopDir "webui"),
  (Join-Path $repoRoot "scripts\windows\Install-LanScreenShare.ps1"),
  (Join-Path $repoRoot "scripts\windows\Uninstall-LanScreenShare.ps1"),
  (Join-Path $repoRoot "scripts\windows\Check-WebView2Runtime.ps1")
)
foreach ($item in $required) {
  if (-not (Test-Path -LiteralPath $item)) {
    Fail "Missing required package input: $item"
  }
}

if (Test-Path -LiteralPath $stageDir) { Remove-Item -LiteralPath $stageDir -Recurse -Force }
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Write-Section "Stage payload"
Copy-Item -Path (Join-Path $desktopDir "*") -Destination $stageDir -Recurse -Force
$stageScriptsDir = Join-Path $stageDir "scripts\windows"
New-Item -ItemType Directory -Force -Path $stageScriptsDir | Out-Null
@(
  "common.ps1",
  "package_common.ps1",
  "Check-WebView2Runtime.ps1",
  "Run-NetworkDiagnostics.ps1",
  "browser_smoke.ps1",
  "validate_release.ps1",
  "smoke_server.ps1"
) | ForEach-Object {
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot $_) -Destination (Join-Path $stageScriptsDir $_) -Force
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "Install-LanScreenShare.ps1") -Destination (Join-Path $stageDir "Install-LanScreenShare.ps1") -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "Uninstall-LanScreenShare.ps1") -Destination (Join-Path $stageDir "Uninstall-LanScreenShare.ps1") -Force

$docSource = Join-Path $repoRoot "docs\WINDOWS_BOOTSTRAP_GUIDE.md"
if (Test-Path -LiteralPath $docSource) {
  Copy-Item -LiteralPath $docSource -Destination (Join-Path $stageDir "WINDOWS_BOOTSTRAP_GUIDE.md") -Force
}

$manifest = [ordered]@{
  product = $global:LanProductName
  version = $versionResolved
  arch = $Arch
  config = $Config
  generated_at = (Get-Date).ToString("s")
  desktop_dir = $desktopDir
  server_dir = $serverDir
  included_helpers = @(
    "Install-LanScreenShare.ps1",
    "Uninstall-LanScreenShare.ps1",
    "scripts/windows/Check-WebView2Runtime.ps1",
    "scripts/windows/Run-NetworkDiagnostics.ps1"
  )
}
$manifest | ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $stageDir "package_manifest.json") -Encoding utf8

Write-Section "Create zip"
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -Force

Write-Host "StageDir: $stageDir" -ForegroundColor Green
Write-Host "ZipPath:  $zipPath" -ForegroundColor Green
