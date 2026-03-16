param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [string]$BindHost = "127.0.0.1",
  [int]$Port = 9443,
  [int]$RunFor = 15,
  [string]$ServerExe = "",
  [string]$WwwRoot = "",
  [string]$CertDir = ""
)

. (Join-Path $PSScriptRoot "common.ps1")

$root = Get-RepoRoot
$exe = if ($ServerExe) { $ServerExe } else { Join-Path $root "out\server\$Config\lan_screenshare_server.exe" }
if (-not (Test-Path $exe)) {
  Fail "Server exe not found: $exe"
}

$resolvedWwwRoot = if ($WwwRoot) { $WwwRoot } else { Join-Path (Split-Path -Parent $exe) "www" }
$resolvedCertDir = if ($CertDir) { $CertDir } else { Join-Path (Split-Path -Parent $exe) "cert" }
if (-not (Test-Path $resolvedWwwRoot)) {
  Fail "www root not found: $resolvedWwwRoot"
}
if (-not (Test-Path $resolvedCertDir)) {
  Fail "cert dir not found: $resolvedCertDir"
}

$curl = Get-Command curl.exe -ErrorAction SilentlyContinue
if (-not $curl) {
  Fail "curl.exe not found. It is required for the smoke check."
}

Write-Section "Start server"
$args = @(
  "--bind", $BindHost,
  "--port", "$Port",
  "--www", $resolvedWwwRoot,
  "--certdir", $resolvedCertDir,
  "--run-for", "$RunFor",
  "--no-stdin"
)
$proc = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden

try {
  Start-Sleep -Seconds 2

  if ($proc.HasExited) {
    Fail "Server exited before smoke request completed."
  }

  Write-Section "Check /health"
  $healthUrl = "https://$BindHost`:$Port/health"
  $health = & $curl.Source -k --silent --show-error --fail $healthUrl
  if ($LASTEXITCODE -ne 0 -or $health -ne "ok") {
    Fail "Health check failed for $healthUrl"
  }

  Write-Host "Smoke check passed: $healthUrl -> ok" -ForegroundColor Green
}
finally {
  if ($proc -and -not $proc.HasExited) {
    try { Stop-Process -Id $proc.Id -Force } catch {}
  }
}
