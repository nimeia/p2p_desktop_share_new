# Windows build scripts

## One-command build

Build both C++ server and desktop host app:

```powershell
.\scripts\build.ps1 -Config Debug -Target all
```

Build only server:

```powershell
.\scripts\build.ps1 -Target server
```

Build only desktop host:

```powershell
.\scripts\build.ps1 -Target desktop_host
```

## Common options

- `-VcpkgRoot <path>`: path to vcpkg (or set `VCPKG_ROOT`)
- `-Generator auto|ninja|vs`
- `-Triplet x64-windows|x64-windows-static`
- `-Clean`: remove CMake build dir before configuring
- `-SanIp <ip>`: SAN IP for self-signed cert (defaults to auto-detected IPv4)

## Run helpers

Run the server after building:

```powershell
.\scripts\windows\run_server.ps1 -Config Debug -Port 9443
```

Run the desktop host app after building:

```powershell
.\scripts\windows\run_desktop_host.ps1 -Config Debug
```


## Debug

Print executed commands:

```powershell
.\scripts\build.ps1 -Target server -VerboseCommands
```


## Doctor

Collect environment and build-dir diagnostics:

```powershell
.\scripts\windows\doctor.ps1
```


## Logs

Each build writes a log file under `out\logs\build_*.log`.


> Note: On Windows, `-Generator auto` now prefers Visual Studio (msbuild) for reliability. Use `-Generator ninja` if you explicitly want Ninja.


## BuildRoot (avoid long paths)

When the repo path is long, MSBuild TryCompile may fail. You can force a short build directory:

```powershell
.\scripts\build.ps1 -Target server -BuildRoot C:\b -Clean
```

By default, the build script automatically switches to a short build root under LocalAppData when it detects a long path.
This default build root is made unique per source folder name to avoid CMake cache mismatch errors when you unzip multiple
source trees and build them without cleaning.
