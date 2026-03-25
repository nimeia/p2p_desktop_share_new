param()

. (Join-Path $PSScriptRoot "common.ps1")

$global:LanProductName = "LanScreenShareHost"
$global:LanPublisher = "LAN Screen Share"
$global:LanUninstallKey = "Software\Microsoft\Windows\CurrentVersion\Uninstall\LanScreenShareHost"

function Get-PackageVersion([string]$RepoRoot, [string]$Version) {
  if ($Version) { return $Version }
  try {
    $gitVersion = (& git -C $RepoRoot rev-parse --short HEAD 2>$null)
    if ($LASTEXITCODE -eq 0 -and $gitVersion) {
      return ("0.1.0+{0}" -f $gitVersion.Trim())
    }
  } catch {}
  return "0.1.0-dev"
}

function Get-InstallRoot([string]$Scope) {
  if ($Scope -eq "machine") {
    return (Join-Path $env:ProgramFiles $global:LanProductName)
  }
  return (Join-Path $env:LOCALAPPDATA ("Programs\" + $global:LanProductName))
}

function New-Shortcut([string]$ShortcutPath, [string]$TargetPath, [string]$Description) {
  $parent = Split-Path -Parent $ShortcutPath
  if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
  $shell = New-Object -ComObject WScript.Shell
  $shortcut = $shell.CreateShortcut($ShortcutPath)
  $shortcut.TargetPath = $TargetPath
  $shortcut.WorkingDirectory = Split-Path -Parent $TargetPath
  $shortcut.Description = $Description
  $shortcut.IconLocation = "$TargetPath,0"
  $shortcut.Save()
}

function Remove-Shortcut([string]$ShortcutPath) {
  if (Test-Path -LiteralPath $ShortcutPath) {
    Remove-Item -LiteralPath $ShortcutPath -Force -ErrorAction SilentlyContinue
  }
}

function Write-UninstallEntry([string]$Scope, [string]$InstallDir, [string]$Version) {
  $root = if ($Scope -eq "machine") { [Microsoft.Win32.Registry]::LocalMachine } else { [Microsoft.Win32.Registry]::CurrentUser }
  $key = $root.CreateSubKey($global:LanUninstallKey)
  if (-not $key) { throw "Unable to create uninstall registry key." }
  $uninstallScript = Join-Path $InstallDir "Uninstall-LanScreenShare.ps1"
  $command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$uninstallScript`" -Scope $Scope"
  $key.SetValue("DisplayName", $global:LanProductName)
  $key.SetValue("Publisher", $global:LanPublisher)
  $key.SetValue("DisplayVersion", $Version)
  $key.SetValue("InstallLocation", $InstallDir)
  $key.SetValue("UninstallString", $command)
  $key.SetValue("QuietUninstallString", $command)
  $key.SetValue("NoModify", 1, [Microsoft.Win32.RegistryValueKind]::DWord)
  $key.SetValue("NoRepair", 0, [Microsoft.Win32.RegistryValueKind]::DWord)
  $key.Close()
}

function Remove-UninstallEntry([string]$Scope) {
  $root = if ($Scope -eq "machine") { [Microsoft.Win32.Registry]::LocalMachine } else { [Microsoft.Win32.Registry]::CurrentUser }
  try { $root.DeleteSubKeyTree($global:LanUninstallKey, $false) } catch {}
}
