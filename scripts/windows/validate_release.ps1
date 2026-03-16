param(
  [ValidateSet("Debug","Release")] [string]$Config = "Release",
  [ValidateSet("x64")] [string]$Arch = "x64",
  [string]$DesktopDir = "",
  [int]$Port = 9543,
  [int]$LaunchSeconds = 6,
  [switch]$SkipServerSmoke,
  [switch]$SkipDesktopLaunch
)

. (Join-Path $PSScriptRoot "common.ps1")

function Assert-PathExists(
  [string]$Path,
  [string]$Description
) {
  if (-not (Test-Path $Path)) {
    Fail "$Description missing: $Path"
  }
}

$root = Get-RepoRoot
if (-not $DesktopDir) {
  $DesktopDir = Join-Path $root ("out\desktop_host\" + $Arch + "\" + $Config)
}

Write-Section "Validate desktop payload layout"
Assert-PathExists $DesktopDir "desktop output directory"
Assert-PathExists (Join-Path $DesktopDir "LanScreenShareHostApp.exe") "desktop host executable"
Assert-PathExists (Join-Path $DesktopDir "lan_screenshare_server.exe") "bundled server executable"
Assert-PathExists (Join-Path $DesktopDir "cert\server.crt") "bundled server certificate"
Assert-PathExists (Join-Path $DesktopDir "cert\server.key") "bundled server key"
Assert-PathExists (Join-Path $DesktopDir "www\host.html") "bundled host page"
Assert-PathExists (Join-Path $DesktopDir "www\viewer.html") "bundled viewer page"
Assert-PathExists (Join-Path $DesktopDir "www\assets\common.js") "bundled common.js"
Assert-PathExists (Join-Path $DesktopDir "webui\index.html") "embedded desktop web UI"
Assert-PathExists (Join-Path $DesktopDir "webui\app.js") "embedded desktop app.js"
Assert-PathExists (Join-Path $DesktopDir "webui\style.css") "embedded desktop style.css"

if (-not $SkipServerSmoke) {
  Write-Section "Smoke test bundled server"
  & (Join-Path $root "scripts\windows\smoke_server.ps1") `
    -ServerExe (Join-Path $DesktopDir "lan_screenshare_server.exe") `
    -WwwRoot (Join-Path $DesktopDir "www") `
    -CertDir (Join-Path $DesktopDir "cert") `
    -BindHost "127.0.0.1" `
    -Port $Port `
    -RunFor 20
}

if (-not $SkipDesktopLaunch) {
  Write-Section "Launch desktop host"
  $exe = Join-Path $DesktopDir "LanScreenShareHostApp.exe"
  $proc = Start-Process -FilePath $exe -WorkingDirectory $DesktopDir -PassThru
  try {
    Start-Sleep -Seconds $LaunchSeconds
    if ($proc.HasExited) {
      Fail "Desktop host exited too early with code $($proc.ExitCode)."
    }
    Write-Host "Desktop host stayed alive for $LaunchSeconds seconds." -ForegroundColor Green
  }
  finally {
    if ($proc -and -not $proc.HasExited) {
      try { Stop-Process -Id $proc.Id -Force } catch {}
    }
  }
}

Write-Host "Desktop release validation passed." -ForegroundColor Green
