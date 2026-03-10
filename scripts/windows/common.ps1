param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Section([string]$Title) {
  Write-Host ""
  Write-Host "==== $Title ====" -ForegroundColor Cyan
}

function Fail([string]$Message, [int]$Code = 1) {
  Write-Host ""
  Write-Host "ERROR: $Message" -ForegroundColor Red
  exit $Code
}

function Ensure-Command([string]$Name, [string]$Hint = "") {
  if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
    if ($Hint) { Fail "Missing '$Name'. $Hint" } else { Fail "Missing '$Name' in PATH." }
  }
}

function Get-RepoRoot {
  $here = Split-Path -Parent $PSScriptRoot
  return (Resolve-Path (Join-Path $here "..")).Path
}

function Find-Vcpkg([string]$VcpkgRoot) {
  $candidates = @()
  if ($VcpkgRoot -and $VcpkgRoot -ne "auto") { $candidates += $VcpkgRoot }
  if ($env:VCPKG_ROOT) { $candidates += $env:VCPKG_ROOT }
  $candidates += @(
    (Join-Path $HOME "vcpkg"),
    "C:\vcpkg",
    "D:\vcpkg",
    "D:\dev\vcpkg"
  )
  foreach ($c in $candidates) {
    if (-not $c) { continue }
    $exe = Join-Path $c "vcpkg.exe"
    if (Test-Path $exe) { return (Resolve-Path $c).Path }
  }
  return $null
}

function Find-Vswhere {
  $p = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $p) { return $p }
  return $null
}

function Find-MSBuild {
  $vswhere = Find-Vswhere
  if (-not $vswhere) { return $null }
  $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2>$null
  if ($LASTEXITCODE -ne 0) { return $null }
  if ($msbuild -is [System.Array]) { return $msbuild[0] }
  return $msbuild
}

function Get-DefaultIPv4 {
  # Best-effort: prefer active non-loopback IPv4 on "Up" adapters.
  try {
    $ips = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop |
      Where-Object { $_.IPAddress -ne "127.0.0.1" -and $_.PrefixOrigin -ne "WellKnown" } |
      Where-Object { $_.InterfaceOperationalStatus -eq "Up" } |
      Sort-Object -Property InterfaceMetric, PrefixLength
    if ($ips -and $ips.Count -gt 0) { return $ips[0].IPAddress }
  } catch {}
  return "127.0.0.1"
}

function Quote-Arg([string]$a) {
  if ($null -eq $a) { return "" }
  # CreateProcess-style quoting (good enough for file paths and simple args)
  $escaped = $a -replace '"', '\"'
  if ($escaped -match '\s') { return '"' + $escaped + '"' }
  return $escaped
}

function Invoke-External(
  [string]$File,
  [string[]]$ArgList,
  [string]$WorkingDir = "",
  [switch]$NoThrow,
  [switch]$Echo,
  [string]$LogFile = ""
) {
  $argString = (($ArgList | ForEach-Object { Quote-Arg $_ }) -join " ")
  $cmd = $File + " " + $argString
  if ($Echo) { Write-Host $cmd -ForegroundColor DarkGray }

  # Use Start-Process with redirection to avoid PowerShell treating native stderr as an error record (PS 5.1 + $ErrorActionPreference=Stop).
  $tmpOut = [System.IO.Path]::GetTempFileName()
  $tmpErr = [System.IO.Path]::GetTempFileName()

  try {
    $startInfo = @{
      FilePath               = $File
      ArgumentList           = $argString
      Wait                   = $true
      PassThru               = $true
      NoNewWindow            = $true
      RedirectStandardOutput = $tmpOut
      RedirectStandardError  = $tmpErr
    }
    if ($WorkingDir) { $startInfo["WorkingDirectory"] = $WorkingDir }

    $p = Start-Process @startInfo
    $code = $p.ExitCode

    $outLines = @()
    $errLines = @()
    if (Test-Path $tmpOut) { $outLines = Get-Content -LiteralPath $tmpOut -ErrorAction SilentlyContinue }
    if (Test-Path $tmpErr) { $errLines = Get-Content -LiteralPath $tmpErr -ErrorAction SilentlyContinue }

    # Print to console
    foreach ($l in $outLines) { Write-Host $l }
    foreach ($l in $errLines) { Write-Host $l }

    # Append to log file if requested
    if ($LogFile) {
      $parent = Split-Path -Parent $LogFile
      if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
      if (@($outLines).Count -gt 0) { $outLines | Out-File -LiteralPath $LogFile -Append -Encoding utf8 }
      if (@($errLines).Count -gt 0) { $errLines | Out-File -LiteralPath $LogFile -Append -Encoding utf8 }
    }

  } finally {
    Remove-Item -LiteralPath $tmpOut -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $tmpErr -Force -ErrorAction SilentlyContinue
  }

  if (-not $NoThrow -and $code -ne 0) {
    throw "Command failed ($code): $cmd"
  }
  return $code
}

