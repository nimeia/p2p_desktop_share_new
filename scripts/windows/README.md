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
- `-SkipBrowserSmoke`: skip the C++ HTTP/WS browser smoke test target
- `-SkipDesktopValidation`: skip desktop payload/startup validation after desktop builds

## Packaging

Create a redistributable Windows package (stage + zip):

```powershell
.\scripts\windows\package.ps1 -Config Release
```

The package includes `Install-LanScreenShare.ps1`, `Uninstall-LanScreenShare.ps1`, the desktop payload, and helper scripts under `scripts/windows/`.

Create a Windows Store / MSIX package:

```powershell
.\scripts\windows\package_store.ps1 -Config Release -IdentityName "YourPartnerCenterIdentity" -Publisher "CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
```

See `docs/WINDOWS_STORE_PACKAGING.md` for the full MSIX/MSIXUPLOAD flow and the current writable-path blocker that still needs to be fixed before Store submission.

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


## Diagnostics helper

Run local network diagnostics helper:

```powershell
.\scripts\windows\Run-NetworkDiagnostics.ps1 -HostIp 192.168.1.20 -Port 9443 -ServerExe .\out\build\windows\Release\lan_screenshare_server.exe -OutputPath .\out\diagnostics\network.txt
```

This collects Windows Firewall profile/rule state, current TCP listeners, and quick reachability probes, then writes a text report and opens it in Notepad.


## Browser smoke

Run the C++ HTTP/WS browser smoke target against the configured build tree:

```powershell
.\scripts\windows\browser_smoke.ps1 -Config Debug
```

## Desktop release validation

Validate the built desktop payload and startup behavior:

```powershell
.\scripts\windows\validate_release.ps1 -Config Release
```

This validates the copied runtime bundle, smoke-tests the bundled server, and verifies that the desktop process does not exit immediately after launch.


## WebView2 helper

```powershell
.\scripts\windows\Check-WebView2Runtime.ps1
```
