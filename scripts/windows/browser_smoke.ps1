param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [ValidateSet("x64-windows","x64-windows-static")] [string]$Triplet = "x64-windows",
  [ValidateSet("auto","ninja","vs")] [string]$Generator = "auto",
  [string]$BuildDir = "auto"
)

. (Join-Path $PSScriptRoot "common.ps1")

$root = Get-RepoRoot
$Generator = Resolve-CmakeGenerator $Generator

if ($BuildDir -eq "auto") {
  $BuildDir = Get-DefaultWindowsBuildDir $root $Generator $Triplet $Config
}

if (-not (Test-Path $BuildDir)) {
  Fail "Build directory not found: $BuildDir"
}

Write-Section "Build browser smoke target"
$buildArgs = @("--build", $BuildDir, "--target", "lan_screenshare_browser_smoke_tests")
if ($Generator -ne "ninja") { $buildArgs += @("--config", $Config) }
Invoke-External "cmake" $buildArgs

Write-Section "Run browser smoke test"
$ctestArgs = @("--test-dir", $BuildDir, "--output-on-failure", "-R", "lan_screenshare_browser_smoke_tests")
if ($Generator -ne "ninja") { $ctestArgs += @("-C", $Config) }
Invoke-External "ctest" $ctestArgs

Write-Host "Browser smoke validation passed." -ForegroundColor Green
