param(
  [string]$CertPath = "",
  [switch]$OpenStore
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "common.ps1")

if (-not $CertPath) {
  $repoRoot = Get-RepoRoot
  $installedCert = Join-Path $repoRoot "cert\server.crt"
  if (Test-Path -LiteralPath $installedCert) {
    $CertPath = $installedCert
  } else {
    $CertPath = Join-Path $repoRoot "out\desktop_host\x64\Release\cert\server.crt"
  }
}

if (-not (Test-Path -LiteralPath $CertPath)) {
  Fail "Certificate file not found: $CertPath"
}

Write-Section "Import local HTTPS certificate"
Write-Host "CertPath: $CertPath"

try {
  $import = Import-Certificate -FilePath $CertPath -CertStoreLocation "Cert:\CurrentUser\Root"
  if (-not $import) {
    Fail "Import-Certificate did not return a result."
  }
  Write-Host "Imported local certificate into Cert:\CurrentUser\Root" -ForegroundColor Green
} catch {
  Fail ("Failed to import certificate: {0}" -f $_.Exception.Message)
}

if ($OpenStore) {
  try { Start-Process certmgr.msc | Out-Null } catch {}
}
