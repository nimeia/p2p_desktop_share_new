# Desktop Host Build Guide

This guide reflects the current Windows desktop-host workflow.

## Build

Common build entry point:

```powershell
.\scripts\build.ps1 -Target desktop_host -Config Debug
```

Build desktop host plus local server:

```powershell
.\scripts\build.ps1 -Target all -Config Debug
```

Advanced Windows-only build entry point:

```powershell
.\scripts\windows\build.ps1 -Target desktop_host -Config Debug -SkipDesktopValidation
```

You can also open and build `src/desktop_host/ViewMeshApp.sln` directly in Visual Studio, but the scripts are the authoritative path because they also copy the bundled runtime layout.

## Run

Run the desktop host:

```powershell
.\scripts\windows\run_desktop_host.ps1 -Config Debug
```

Run the local server directly:

```powershell
.\scripts\windows\run_server.ps1 -Config Debug -Port 9443
```

Manual server run:

```powershell
.\out\server\Debug\ViewMeshServer.exe --bind 0.0.0.0 --host-ip auto --port 9443 --www .\out\server\Debug\www --admin-www .\out\server\Debug\webui
```

## Outputs

- server: `out\server\<Config>\`
- desktop host: `out\desktop_host\<Arch>\<Config>\`
- share bundle: `out\share_bundle\`

The desktop output bundles:

- `ViewMesh.exe`
- `ViewMeshServer.exe`
- `www\`
- `webui\`

## Runtime notes

- transport is plain HTTP / WS only
- the old local certificate/bootstrap flow is no longer part of the desktop runtime
- WebView2 is optional; if it is unavailable, the desktop host can still open the host page in an external browser

## Validation helpers

```powershell
.\scripts\windows\smoke_server.ps1 -Config Debug
.\scripts\windows\browser_smoke.ps1 -Config Debug
.\scripts\windows\validate_release.ps1 -Config Release
.\scripts\windows\Check-WebView2Runtime.ps1
```
