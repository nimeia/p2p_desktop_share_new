# Windows Packaging

The repository now includes a repeatable Windows zip-package flow for the desktop host.

## Scripts

- `scripts/windows/package.ps1`
  - builds or reuses the current desktop/server outputs
  - validates the desktop payload layout
  - stages a redistributable package under `out/package/windows/`
  - emits `package_manifest.json`
  - creates a zip archive for handoff
- `scripts/windows/Install-ViewMesh.ps1`
  - installs or upgrades a staged package
  - supports `-Scope user` and `-Scope machine`
  - writes an uninstall registry entry
  - creates a Start Menu shortcut
- `scripts/windows/Uninstall-ViewMesh.ps1`
  - removes the installed payload
  - optionally removes local user data

Store/MSIX packaging is documented separately in [WINDOWS_STORE_PACKAGING.md](WINDOWS_STORE_PACKAGING.md).

## Typical flow

Build and package from Release output:

```powershell
.\scripts\windows\package.ps1 -Config Release
```

Reuse an already-built Release payload:

```powershell
.\scripts\windows\package.ps1 -Config Release -SkipBuild
```

Install the staged package for the current user:

```powershell
.\out\package\windows\ViewMesh_<version>_win-x64\Install-ViewMesh.ps1 -Scope user
```

Install for all users:

```powershell
.\out\package\windows\ViewMesh_<version>_win-x64\Install-ViewMesh.ps1 -Scope machine
```

Uninstall later:

```powershell
powershell.exe -ExecutionPolicy Bypass -File "<InstallDir>\Uninstall-ViewMesh.ps1" -Scope user
```

## Package contents

The staged package preserves the runtime layout the desktop host already expects:

- `ViewMesh.exe`
- `ViewMeshServer.exe`
- `www/`
- `webui/`
- `Install-ViewMesh.ps1`
- `Uninstall-ViewMesh.ps1`
- `WINDOWS_BOOTSTRAP_GUIDE.md`
- `scripts/windows/`

The helper folder currently includes:

- `Check-WebView2Runtime.ps1`
- `Run-NetworkDiagnostics.ps1`
- `smoke_server.ps1`
- `validate_release.ps1`
- shared PowerShell helper files

No `cert/` directory or local trust-import helper is part of the current package baseline.

## Notes

- The package format is a staged directory plus zip archive, not MSI.
- The desktop output already bundles `ViewMeshServer.exe`, `www/`, and `webui/`, so the packaged app can be moved to another machine without rebuilding the runtime tree.
- Clean-machine install/upgrade/uninstall validation is still required before treating this as a release-hardened distribution flow.
