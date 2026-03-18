param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [int]$Port = 9443,
  [string]$Bind = "0.0.0.0",
  [string]$SanIp = ""
)

. (Join-Path $PSScriptRoot "common.ps1")

$root = Get-RepoRoot
$outDir = Join-Path $root "out"
$exe = Join-Path $outDir "server\$Config\lan_screenshare_server.exe"
if (-not (Test-Path $exe)) { Fail "Server exe not found. Build first: scripts\windows\build.ps1 -Target server" }

if (-not $SanIp) { $SanIp = Get-DefaultIPv4 }
$serverDir = Split-Path -Parent $exe
$wwwDir = Join-Path $serverDir "www"
$adminDir = Join-Path $serverDir "webui"

if (-not (Test-Path (Join-Path $wwwDir "host.html"))) { Fail "Server www not found: $wwwDir. Rebuild with scripts\\build.ps1 -Target server" }
if (-not (Test-Path (Join-Path $adminDir "index.html"))) { Fail "Server admin webui not found: $adminDir. Rebuild with scripts\\build.ps1 -Target server" }

Write-Section "Run server"
Write-Host "Admin: http://127.0.0.1`:$Port/admin/"
Write-Host "Host:  http://127.0.0.1`:$Port/host?room=test&token=test"
Write-Host "View:  http://$SanIp`:$Port/view?room=test"
& $exe --bind $Bind --port $Port --www $wwwDir --admin-www $adminDir --san-ip $SanIp
