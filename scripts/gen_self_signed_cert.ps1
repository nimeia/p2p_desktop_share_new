param(
  [string]$OutDir = "$(Join-Path $PSScriptRoot '..\cert')",
  [string]$CommonName = "lan-screenshare",
  [string]$SanIp = "127.0.0.1",
  # Optional: explicitly specify openssl.exe path.
  [string]$OpenSSL = "",
  # Optional: vcpkg root. If not provided, will try env:VCPKG_ROOT.
  [string]$VcpkgRoot = "",
  # vcpkg triplet used to locate openssl tool under installed/<triplet>/tools/openssl.
  [string]$Triplet = "x64-windows"
)

function Resolve-OpenSSL {
  param(
    [string]$OpenSSL,
    [string]$VcpkgRoot,
    [string]$Triplet
  )

  if ($OpenSSL) {
    if (Test-Path $OpenSSL) { return (Resolve-Path $OpenSSL).Path }
    throw "OpenSSL path provided but not found: $OpenSSL"
  }

  $cmd = Get-Command openssl -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }

  # Try vcpkg tool (preferred for this repo)
  if (-not $VcpkgRoot) { $VcpkgRoot = $env:VCPKG_ROOT }
  if ($VcpkgRoot) {
    $candidate1 = Join-Path $VcpkgRoot ("installed\\$Triplet\\tools\\openssl\\openssl.exe")
    if (Test-Path $candidate1) { return (Resolve-Path $candidate1).Path }
    # Some layouts may place tools under installed/<triplet>/tools/openssl/bin
    $candidate2 = Join-Path $VcpkgRoot ("installed\\$Triplet\\tools\\openssl\\bin\\openssl.exe")
    if (Test-Path $candidate2) { return (Resolve-Path $candidate2).Path }
  }

  # Common Windows locations (best-effort)
  $candidates = @(
    (Join-Path $env:ProgramFiles "OpenSSL-Win64\\bin\\openssl.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "OpenSSL-Win32\\bin\\openssl.exe"),
    (Join-Path $env:ProgramFiles "Git\\usr\\bin\\openssl.exe"),
    "C:\\msys64\\usr\\bin\\openssl.exe",
    "C:\\Program Files\\Git\\usr\\bin\\openssl.exe"
  )
  foreach ($c in $candidates) {
    if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path }
  }

  return $null
}

$openssl = Resolve-OpenSSL -OpenSSL $OpenSSL -VcpkgRoot $VcpkgRoot -Triplet $Triplet
if (-not $openssl) {
  Write-Host "OpenSSL not found." -ForegroundColor Red
  Write-Host "\nOptions:" -ForegroundColor Yellow
  Write-Host "  1) Install OpenSSL and ensure it is in PATH (recommended):" -ForegroundColor Yellow
  Write-Host "     winget install -e --id ShiningLight.OpenSSL" -ForegroundColor DarkGray
  Write-Host "  2) Or use vcpkg-provided openssl tool (this repo already depends on 'openssl'):" -ForegroundColor Yellow
  Write-Host "     Ensure vcpkg install succeeded, then set VCPKG_ROOT or pass -VcpkgRoot." -ForegroundColor DarkGray
  throw "OpenSSL is required to generate PEM key/cert (server.key/server.crt)."
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$key = Join-Path $OutDir "server.key"
$crt = Join-Path $OutDir "server.crt"
$cnf = Join-Path $OutDir "openssl.cnf"

@"
[req]
default_bits       = 2048
distinguished_name = req_distinguished_name
req_extensions     = v3_req
prompt             = no

[req_distinguished_name]
CN = $CommonName

[v3_req]
keyUsage = keyEncipherment, dataEncipherment, digitalSignature
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
IP.1 = $SanIp
DNS.1 = localhost
"@ | Set-Content -Encoding ascii $cnf

& $openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout $key -out $crt -config $cnf
Write-Host "Generated: $key"
Write-Host "Generated: $crt"
