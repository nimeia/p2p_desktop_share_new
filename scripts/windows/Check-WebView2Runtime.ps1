param(
  [string]$OutputPath = ""
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "common.ps1")

function Get-WebView2Version([Microsoft.Win32.RegistryHive]$Hive, [string]$SubKey) {
  try {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey($Hive, [Microsoft.Win32.RegistryView]::Registry32)
    $key = $base.OpenSubKey($SubKey)
    if (-not $key) { return $null }
    $value = [string]$key.GetValue("pv", $null)
    if ([string]::IsNullOrWhiteSpace($value) -or $value -eq "0.0.0.0") { return $null }
    return $value
  } catch {
    return $null
  }
}

$guid = "{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
$machineKey = if ([Environment]::Is64BitOperatingSystem) {
  "SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\$guid"
} else {
  "SOFTWARE\Microsoft\EdgeUpdate\Clients\$guid"
}
$userKey = "Software\Microsoft\EdgeUpdate\Clients\$guid"
$machineVersion = Get-WebView2Version ([Microsoft.Win32.RegistryHive]::LocalMachine) $machineKey
$userVersion = Get-WebView2Version ([Microsoft.Win32.RegistryHive]::CurrentUser) $userKey
$installed = $false
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("LAN Screen Share WebView2 runtime check")
$lines.Add(("Generated: {0}" -f (Get-Date).ToString("s")))
$lines.Add("")
$lines.Add(("Machine install: {0}" -f ($(if ($machineVersion) { $machineVersion } else { "not found" }))))
$lines.Add(("Current-user install: {0}" -f ($(if ($userVersion) { $userVersion } else { "not found" }))))
if ($machineVersion -or $userVersion) {
  $installed = $true
  $lines.Add("")
  $lines.Add("Result: Evergreen WebView2 Runtime detected.")
  $lines.Add("Recommendation: Keep the Evergreen runtime and let it update automatically.")
} else {
  $lines.Add("")
  $lines.Add("Result: Evergreen WebView2 Runtime not detected.")
  $lines.Add("Recommendation: Install or repair the Evergreen runtime, then relaunch LanScreenShareHostApp.")
  $lines.Add("You can use either the WebView2 bootstrapper (online) or the standalone installer (offline), depending on the target machine.")
}

$text = ($lines -join [Environment]::NewLine)
Write-Host $text

if (-not $OutputPath) {
  $repoRoot = Get-RepoRoot
  $reportDir = Join-Path $repoRoot "out\diagnostics"
  New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
  $OutputPath = Join-Path $reportDir ("webview2_runtime_{0}.txt" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
}

$parent = Split-Path -Parent $OutputPath
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$text | Out-File -LiteralPath $OutputPath -Encoding utf8
Write-Host "Saved: $OutputPath"
try { Start-Process notepad.exe -ArgumentList @($OutputPath) | Out-Null } catch {}
if (-not $installed) { exit 2 }
exit 0
