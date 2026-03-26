# ViewMesh

ViewMesh is a Windows-first local screen sharing app. The current repository combines:

- a Win32 desktop host shell with optional embedded WebView2
- a local C++ HTTP / WS service built on Boost.Asio/Beast
- browser host/viewer pages that use WebRTC for media transport
- Windows packaging plus early Linux/macOS native-shell packaging baselines

## Current baseline

The repository is no longer a scaffold. Today it already provides:

- local service routes for `/host`, `/view`, `/admin/`, `/assets/*`, `/health`, `/api/status`, and `/ws`
- a desktop host that manages the local server, LAN diagnostics, share bundle export, and operator-facing health guidance
- browser-based host capture via `getDisplayMedia` and recvonly viewer playback
- exported offline/share diagnostics artifacts under `out/share_bundle/`
- Windows zip packaging, install/uninstall scripts, and Store/MSIX container generation
- Linux tray and macOS menu-bar packaging baselines

Important current runtime facts:

- transport is plain HTTP / WS only
- the old local certificate bootstrap flow is no longer part of the runtime
- WebView2 is optional for embedded admin/host preview; browser fallback remains supported

## Quick start

### Windows build

Recommended shell: Visual Studio 2022 Developer PowerShell

Prerequisites:

- Visual Studio 2022 with the C++ desktop workload
- CMake 3.24+
- vcpkg

Build everything:

```powershell
.\scripts\build.ps1 -Config Debug -Target all
```

Build only the server:

```powershell
.\scripts\build.ps1 -Config Debug -Target server
```

Build only the desktop host:

```powershell
.\scripts\build.ps1 -Config Debug -Target desktop_host
```

Useful outputs:

- `out/server/Debug/ViewMeshServer.exe`
- `out/desktop_host/x64/Debug/ViewMesh.exe`
- `out/logs/build_*.log`

Validation helpers:

```powershell
.\scripts\windows\smoke_server.ps1 -Config Debug
.\scripts\windows\browser_smoke.ps1 -Config Debug
.\scripts\windows\validate_release.ps1 -Config Release
```

### Cross-platform packaging

Linux build:

```bash
./scripts/build_linux.sh --target all --run-tests --bootstrap-vcpkg
```

Linux package:

```bash
./scripts/package_linux.sh --config Release --vcpkg-root "$VCPKG_ROOT"
```

macOS package:

```bash
./scripts/package_macos.sh --config Release --vcpkg-root "$VCPKG_ROOT"
```

Windows package:

```powershell
.\scripts\windows\package.ps1 -Config Release
```

## Documentation

Start with the [documentation index](docs/README.md).

The most useful current entry points are:

- [Windows build guide](docs/BUILD_WINDOWS.md)
- [Windows packaging](docs/WINDOWS_PACKAGING.md)
- [Windows Store packaging](docs/WINDOWS_STORE_PACKAGING.md)
- [Windows bootstrap / operator checks](docs/WINDOWS_BOOTSTRAP_GUIDE.md)
- [Release validation](docs/RELEASE_VALIDATION.md)
- [Windows script reference](scripts/windows/README.md)

## Notes

- `scripts/build.ps1` is a thin wrapper. Use `scripts/windows/build.ps1` when you need advanced Windows-only switches such as `-SkipBrowserSmoke`, `-SkipDesktopValidation`, or `-SkipDesktopHostRestore`.
- Windows builds are incremental by default. Use `-Clean` deliberately when you need a fresh configure/build.
- Some plan/WIP documents under `docs/` are historical snapshots. They may mention completed refactors or removed flows such as local TLS/certificate bootstrap.
