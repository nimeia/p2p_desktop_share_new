param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [string]$BindHost = "127.0.0.1",
  [int]$Port = 9443,
  [int]$RunFor = 15
)

. (Join-Path $PSScriptRoot "common.ps1")

$root = Get-RepoRoot
$exe = Join-Path $root "out\bin\$Config\lan_screenshare_server.exe"
if (-not (Test-Path $exe)) {
  Fail "Server exe not found: $exe"
}

$curl = Get-Command curl.exe -ErrorAction SilentlyContinue
if (-not $curl) {
  Fail "curl.exe not found. It is required for the smoke check."
}

Write-Section "Start server"
$args = @("--bind", $BindHost, "--port", "$Port", "--run-for", "$RunFor", "--no-stdin")
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
