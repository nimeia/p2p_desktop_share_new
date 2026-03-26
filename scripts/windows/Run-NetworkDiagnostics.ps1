[CmdletBinding()]
param(
  [string]$HostIp = "",
  [int]$Port = 9443,
  [string]$ServerExe = "",
  [string]$OutputPath = ""
)

$ErrorActionPreference = 'Stop'

function Add-Line {
  param([System.Collections.Generic.List[string]]$Lines, [string]$Text = "")
  $Lines.Add($Text) | Out-Null
}

function Test-Command {
  param([string]$Name)
  return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-FirewallRuleSummary {
  param([string]$ExePath, [int]$TcpPort)

  if (-not (Test-Command 'Get-NetFirewallRule')) {
    return [pscustomobject]@{
      Ready = $false
      Detail = 'Get-NetFirewallRule is unavailable on this machine.'
      Matches = @()
    }
  }

  $activeProfiles = @()
  if (Test-Command 'Get-NetFirewallProfile') {
    $activeProfiles = @(Get-NetFirewallProfile -ErrorAction SilentlyContinue | Where-Object Enabled)
  }

  $candidateRules = @(Get-NetFirewallRule -Direction Inbound -Action Allow -Enabled True -ErrorAction SilentlyContinue)
  $matches = New-Object System.Collections.Generic.List[string]

  foreach ($rule in $candidateRules) {
    $matched = $false

    try {
      $appFilter = Get-NetFirewallApplicationFilter -AssociatedNetFirewallRule $rule -ErrorAction SilentlyContinue
      foreach ($entry in @($appFilter)) {
        if (-not [string]::IsNullOrWhiteSpace($entry.Program) -and -not [string]::IsNullOrWhiteSpace($ExePath)) {
          if ([string]::Equals($entry.Program, $ExePath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $matched = $true
          }
        }
      }
    } catch {}

    try {
      $portFilter = Get-NetFirewallPortFilter -AssociatedNetFirewallRule $rule -ErrorAction SilentlyContinue
      foreach ($entry in @($portFilter)) {
        if ($entry.Protocol -eq 'TCP' -or $entry.Protocol -eq 6 -or $entry.Protocol -eq 'Any') {
          $rawPorts = @($entry.LocalPort)
          foreach ($rawPort in $rawPorts) {
            if ($null -eq $rawPort) { continue }
            foreach ($token in ($rawPort.ToString() -split ',')) {
              $trimmed = $token.Trim()
              if ($trimmed -eq 'Any' -or $trimmed -eq '*') {
                $matched = $true
                break
              }
              if ($trimmed -match '^(\d+)-(\d+)$') {
                $start = [int]$Matches[1]
                $end = [int]$Matches[2]
                if ($TcpPort -ge $start -and $TcpPort -le $end) {
                  $matched = $true
                  break
                }
              } elseif ($trimmed -match '^\d+$' -and [int]$trimmed -eq $TcpPort) {
                $matched = $true
                break
              }
            }
            if ($matched) { break }
          }
        }
        if ($matched) { break }
      }
    } catch {}

    if ($matched) {
      $matches.Add($rule.DisplayName) | Out-Null
    }
  }

  $detail = if ($matches.Count -gt 0) {
    "Inbound allow rules detected: $($matches -join '; ')"
  } elseif ($activeProfiles.Count -eq 0) {
    'No enabled Windows Firewall profile was detected.'
  } else {
    "No enabled inbound allow rule was matched for port $TcpPort or the current server executable."
  }

  return [pscustomobject]@{
    Ready = ($matches.Count -gt 0 -or $activeProfiles.Count -eq 0)
    Detail = $detail
    Matches = $matches
  }
}

$lines = New-Object 'System.Collections.Generic.List[string]'
Add-Line $lines 'ViewMesh - Network Diagnostics'
Add-Line $lines "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Add-Line $lines "Host IP: $HostIp"
Add-Line $lines "Port: $Port"
Add-Line $lines "Server EXE: $ServerExe"
Add-Line $lines ''

Add-Line $lines 'Firewall profiles'
Add-Line $lines '-----------------'
if (Test-Command 'Get-NetFirewallProfile') {
  $profiles = @(Get-NetFirewallProfile -ErrorAction SilentlyContinue)
  if ($profiles.Count -eq 0) {
    Add-Line $lines '- No firewall profile information was returned.'
  } else {
    foreach ($profile in $profiles) {
      Add-Line $lines ("- {0}: Enabled={1} DefaultInboundAction={2}" -f $profile.Name, $profile.Enabled, $profile.DefaultInboundAction)
    }
  }
} else {
  Add-Line $lines '- Get-NetFirewallProfile is unavailable on this machine.'
}
Add-Line $lines ''

Add-Line $lines 'Firewall inbound allow path'
Add-Line $lines '--------------------------'
$firewallSummary = Get-FirewallRuleSummary -ExePath $ServerExe -TcpPort $Port
Add-Line $lines ("Ready: {0}" -f ($(if ($firewallSummary.Ready) { 'yes' } else { 'no' })))
Add-Line $lines ("Detail: {0}" -f $firewallSummary.Detail)
Add-Line $lines ''

Add-Line $lines 'TCP listeners'
Add-Line $lines '-------------'
if (Test-Command 'Get-NetTCPConnection') {
  $listeners = @(Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue | Where-Object { $_.LocalPort -eq $Port })
  if ($listeners.Count -eq 0) {
    Add-Line $lines ("- No current listener was found for TCP port {0}." -f $Port)
  } else {
    foreach ($listener in $listeners) {
      Add-Line $lines ("- {0}:{1} PID={2}" -f $listener.LocalAddress, $listener.LocalPort, $listener.OwningProcess)
    }
  }
} else {
  Add-Line $lines '- Get-NetTCPConnection is unavailable on this machine.'
}
Add-Line $lines ''

Add-Line $lines 'PowerShell probes'
Add-Line $lines '-----------------'
if (Test-Command 'Test-NetConnection') {
  $targets = New-Object System.Collections.Generic.List[string]
  $targets.Add('127.0.0.1') | Out-Null
  if (-not [string]::IsNullOrWhiteSpace($HostIp) -and $HostIp -ne '(not found)' -and $HostIp -ne '0.0.0.0' -and $HostIp -ne '127.0.0.1') {
    $targets.Add($HostIp) | Out-Null
  }
  foreach ($target in $targets) {
    try {
      $probe = Test-NetConnection -ComputerName $target -Port $Port -InformationLevel Quiet -WarningAction SilentlyContinue
      Add-Line $lines ("- TCP {0}:{1} => {2}" -f $target, $Port, ($(if ($probe) { 'reachable' } else { 'blocked or no response' })))
    } catch {
      Add-Line $lines ("- TCP {0}:{1} => probe failed: {2}" -f $target, $Port, $_.Exception.Message)
    }
  }
} else {
  Add-Line $lines '- Test-NetConnection is unavailable on this machine.'
}
Add-Line $lines ''

Add-Line $lines 'Suggested next actions'
Add-Line $lines '----------------------'
if (-not $firewallSummary.Ready) {
  Add-Line $lines '- Open Windows Firewall settings and create or enable an inbound allow rule for the server executable or TCP port.'
}
Add-Line $lines '- Keep the viewer device on the same LAN / hotspot as the host.'
Add-Line $lines '- Paste the Viewer URL directly into another browser on the LAN to verify the path outside the desktop shell.'

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
  $OutputPath = Join-Path (Get-Location) 'network_diagnostics.txt'
}

$outputDir = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDir)) {
  New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}

$lines | Set-Content -Path $OutputPath -Encoding UTF8
Write-Host "Network diagnostics saved to $OutputPath"

if (Test-Command 'notepad.exe') {
  Start-Process notepad.exe -ArgumentList @($OutputPath) | Out-Null
}
