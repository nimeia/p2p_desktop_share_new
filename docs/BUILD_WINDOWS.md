# Build guide (Windows)

This repo provides PowerShell scripts under `scripts/` to build the C++ server and the desktop host app.

## Prerequisites

- Visual Studio 2022 (Desktop development with C++) or Build Tools
- CMake (>= 3.24)
- vcpkg (manifest mode) with `VCPKG_ROOT` set
- (Optional) Ninja (faster builds)

## Build

Build everything:

```powershell
.\scripts\build.ps1 -Target all -Config Debug
```

Build only server:

```powershell
.\scripts\build.ps1 -Target server -Config Debug
```

The server target now builds the cross-platform CLI entry at `apps/server_cli/main.cpp` and composes platform providers from `src/platform/*`.

Build only desktop host:

```powershell
.\scripts\build.ps1 -Target desktop_host -Config Debug
```

Artifacts:

- Server exe: `out/server/<Config>/lan_screenshare_server.exe`
- Cert: `out/server/<Config>/cert/server.key|server.crt`
- Web root: `out/server/<Config>/www/*`
- Desktop host exe: `out/desktop_host/<Arch>/<Config>/LanScreenShareHostApp.exe`
- Windows package stage/zip: `out/package/windows/` via `scripts/windows/package.ps1`

## Run

```powershell
.\scripts\windows\run_server.ps1 -Config Debug -Port 9443
```

Then open:

- Host: `https://<ip>:9443/host?room=test&token=test`
- Viewer: `https://<ip>:9443/view?room=test`

## Path length on Windows

If CMake fails during TryCompile (MSB6003/DirectoryNotFoundException under CMakeScratch), use a short build root:

```powershell
.\scripts\build.ps1 -Target server -Config Debug -Clean -BuildRoot C:\b -VcpkgRoot "D:\dev\vcpkg"
```

By default the scripts automatically switch to a short build root under LocalAppData when the repo path is long.
The build root is made unique per source directory (based on the repo folder name) to avoid CMake cache mismatch errors
when you unzip multiple "fix" packages side by side.


You can now omit `-SanIp` in the scripts or pass `--san-ip auto` directly to the executable; the CLI will resolve an advertised host IP through the current platform provider and fall back to loopback when discovery is unavailable.

## Package

```powershell
.\scripts\windows\package.ps1 -Config Release
```

## Bootstrap helpers

```powershell
.\scripts\windows\Check-WebView2Runtime.ps1
.\scripts\windows\Trust-LocalCertificate.ps1 -CertPath .\out\desktop_host\x64\Release\cert\server.crt
```
