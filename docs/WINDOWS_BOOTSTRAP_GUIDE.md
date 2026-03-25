# Windows Bootstrap Guide

This guide covers the operator-visible checks that still matter on the current Windows baseline:

1. WebView2 runtime availability for the embedded admin/host preview
2. local network reachability and firewall diagnostics

The old local TLS/certificate trust step is no longer part of the runtime. The desktop host and exported share bundle now run over plain HTTP / WS only.

## WebView2 runtime

The desktop host expects Evergreen WebView2 Runtime when it embeds the HTML admin or host preview.

Run the helper:

```powershell
.\scripts\windows\Check-WebView2Runtime.ps1
```

Packaged builds include the same helper under `scripts/windows/Check-WebView2Runtime.ps1`.

Expected outcomes:

- runtime detected: embedded admin/host preview can initialize when the rest of the environment is healthy
- runtime missing: install or repair Evergreen WebView2 Runtime, then relaunch the desktop host

If WebView2 is unavailable, the product can still fall back to opening the host page in an external browser.

## Network diagnostics

Run the local diagnostics helper when the server starts but other devices cannot reach it, or when Windows Firewall / port conflicts are suspected:

```powershell
.\scripts\windows\Run-NetworkDiagnostics.ps1 -HostIp 192.168.1.20 -Port 9443
```

The helper collects:

- Windows Firewall profile and rule state
- current TCP listeners
- quick reachability probes
- a text report saved under `out\diagnostics\`

Packaged builds include the same helper under `scripts/windows/Run-NetworkDiagnostics.ps1`.

## Operator checklist

- confirm the desktop host or local server is running
- confirm the viewer is using the LAN Viewer URL shown by the desktop host
- if embedded preview is blank, run `Check-WebView2Runtime.ps1`
- if remote devices cannot connect, run `Run-NetworkDiagnostics.ps1`

No local certificate import, trust helper, or transport bypass step is required on the current plain HTTP / WS baseline.
