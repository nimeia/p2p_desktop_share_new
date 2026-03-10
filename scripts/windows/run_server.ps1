param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [int]$Port = 9443,
  [string]$Bind = "0.0.0.0",
  [string]$SanIp = ""
)

. (Join-Path $PSScriptRoot "common.ps1")

$root = Get-RepoRoot
$outDir = Join-Path $root "out"
$exe = Join-Path $outDir "bin\$Config\lan_screenshare_server.exe"
if (-not (Test-Path $exe)) { Fail "Server exe not found. Build first: scripts\windows\build.ps1 -Target server" }

if (-not $SanIp) { $SanIp = Get-DefaultIPv4 }
$serverDir = Split-Path -Parent $exe
$certDir = Join-Path $serverDir "cert"
$wwwDir = Join-Path $serverDir "www"

if (-not (Test-Path (Join-Path $certDir "server.crt"))) { Fail "Server cert not found: $certDir. Rebuild with scripts\\build.ps1 -Target server" }
if (-not (Test-Path (Join-Path $wwwDir "host.html"))) { Fail "Server www not found: $wwwDir. Rebuild with scripts\\build.ps1 -Target server" }

Write-Section "Run server"
Write-Host "Open: https://$SanIp`:$Port/host?room=test&token=test"
Write-Host "View: https://$SanIp`:$Port/view?room=test"
& $exe --bind $Bind --port $Port --www $wwwDir --certdir $certDir --san-ip $SanIp
