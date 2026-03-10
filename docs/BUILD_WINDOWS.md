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

Build only desktop host:

```powershell
.\scripts\build.ps1 -Target desktop_host -Config Debug
```

Artifacts:

- Server exe: `out/bin/<Config>/lan_screenshare_server.exe`
- Cert: `out/cert/server.key|server.crt`
- Web root: `out/www/*`
- Desktop host exe: `desktop_host/LanScreenShareHostApp/bin/<Arch>/<Config>/LanScreenShareHostApp.exe`

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
