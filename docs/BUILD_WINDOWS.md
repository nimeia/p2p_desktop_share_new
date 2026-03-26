# Windows Build Guide

This repository has two Windows build entry points:

- `scripts/build.ps1`: thin wrapper for common day-to-day builds
- `scripts/windows/build.ps1`: full Windows build script with validation and advanced switches

## Prerequisites

- Visual Studio 2022 with the C++ desktop workload
- CMake 3.24+
- vcpkg in manifest mode
- optional: Ninja

## Common builds

Build everything:

```powershell
.\scripts\build.ps1 -Target all -Config Debug
```

Build only the local server:

```powershell
.\scripts\build.ps1 -Target server -Config Debug
```

Build only the desktop host:

```powershell
.\scripts\build.ps1 -Target desktop_host -Config Debug
```

The local server is built from `apps/server_cli/main.cpp` and now serves plain HTTP / WS. The old local certificate bootstrap flow is no longer part of the build/runtime path.

## Advanced Windows-only options

Use the full script when you need switches that the thin wrapper does not expose:

```powershell
.\scripts\windows\build.ps1 -Config Debug -Target all -SkipBrowserSmoke
.\scripts\windows\build.ps1 -Config Debug -Target desktop_host -SkipDesktopValidation
.\scripts\windows\build.ps1 -Config Debug -Target desktop_host -SkipDesktopHostRestore
```

Useful options on `scripts/windows/build.ps1`:

- `-Generator auto|vs|ninja`
- `-Triplet x64-windows|x64-windows-static`
- `-VcpkgRoot <path>`
- `-BuildRoot <path>|auto`
- `-SkipServerSmoke`
- `-SkipBrowserSmoke`
- `-SkipDesktopValidation`
- `-SkipDesktopHostRestore`

Windows builds are incremental by default. Use `-Clean` only when you need a fresh configure/build.

## Outputs

- server exe: `out/server/<Config>/ViewMeshServer.exe`
- server web assets: `out/server/<Config>/www/` and `out/server/<Config>/webui/`
- desktop host exe: `out/desktop_host/<Arch>/<Config>/ViewMesh.exe`
- desktop bundled runtime: `out/desktop_host/<Arch>/<Config>/ViewMeshServer.exe`, `www/`, `webui/`
- build logs: `out/logs/build_*.log`

## Run

Run the local server:

```powershell
.\scripts\windows\run_server.ps1 -Config Debug -Port 9443
```

Run the desktop host:

```powershell
.\scripts\windows\run_desktop_host.ps1 -Config Debug
```

Manual server run:

```powershell
.\out\server\Release\ViewMeshServer.exe --bind 0.0.0.0 --host-ip auto --port 9443 --www .\out\server\Release\www --admin-www .\out\server\Release\webui
```

Client URLs:

- host: `http://<host-ip>:9443/host?room=test&token=test`
- viewer: `http://<host-ip>:9443/view?room=test`
- admin: `http://127.0.0.1:9443/admin/`

## Validation helpers

```powershell
.\scripts\windows\smoke_server.ps1 -Config Debug
.\scripts\windows\browser_smoke.ps1 -Config Debug
.\scripts\windows\validate_release.ps1 -Config Release
.\scripts\windows\doctor.ps1
```

## Path-length fallback

If CMake/MSBuild hits long-path issues during `TryCompile`, force a short build root:

```powershell
.\scripts\windows\build.ps1 -Target server -Config Debug -BuildRoot C:\b -Clean
```

When the repo path is long, the build script can also auto-switch to a short cache root under LocalAppData.

## Packaging

Zip package:

```powershell
.\scripts\windows\package.ps1 -Config Release
```

Store/MSIX package:

```powershell
.\scripts\windows\package_store.ps1 -Config Release -IdentityName "YourPartnerCenterIdentity" -Publisher "CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
```

## Operator helpers

```powershell
.\scripts\windows\Check-WebView2Runtime.ps1
.\scripts\windows\Run-NetworkDiagnostics.ps1
```
