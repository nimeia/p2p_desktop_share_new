# Windows Packaging Baseline

The repository now includes a repeatable Windows-first packaging baseline for the desktop host.

## Scripts

- `scripts/windows/package.ps1`
  - builds or reuses the current desktop/server outputs
  - validates the desktop payload layout
  - stages a redistributable package under `out/package/windows/`
  - emits `package_manifest.json`
  - produces a zip archive for handoff
- `scripts/windows/package_store.ps1`
  - builds or reuses the current desktop/server outputs
  - writes a Store-ready `AppxManifest.xml`
  - generates fallback PNG logo assets when you do not provide Store artwork
  - produces `.msix`, `.msixupload`, and optional `.msixsym` artifacts under `out/package/windows-store/`
- `scripts/windows/Install-LanScreenShare.ps1`
  - installs or upgrades the staged package
  - supports `-Scope user` and `-Scope machine`
  - writes a Windows uninstall registry entry
  - creates a Start Menu shortcut
- `scripts/windows/Uninstall-LanScreenShare.ps1`
  - removes the installed payload
  - optionally removes local user data

## Typical flow

Build + package from a clean Release output:

```powershell
.\scripts\windows\package.ps1 -Config Release
```

Reuse an already validated Release build:

```powershell
.\scripts\windows\package.ps1 -Config Release -SkipBuild
```

Build a Windows Store package:

```powershell
.\scripts\windows\package_store.ps1 `
  -Config Release `
  -IdentityName "YourPartnerCenterIdentity" `
  -Publisher "CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
```

Install the staged package for the current user:

```powershell
.\out\package\windows\LanScreenShareHost_<version>_win-x64\Install-LanScreenShare.ps1 -Scope user
```

Uninstall later:

```powershell
powershell.exe -ExecutionPolicy Bypass -File "<InstallDir>\Uninstall-LanScreenShare.ps1" -Scope user
```

## Package contents

The staged package keeps the payload layout that the desktop app already expects:

- `LanScreenShareHostApp.exe`
- `lan_screenshare_server.exe`
- `cert/`
- `www/`
- `webui/`
- `scripts/windows/` helper scripts for runtime checks, certificate trust, and network diagnostics

That keeps the desktop host portable across machines without asking the operator to manually rebuild a runtime tree.

## Store packaging note

The new MSIX script is suitable for generating the package container, but the current desktop host still writes diagnostics and share bundle output under `AppDir()\out\...`.

An installed MSIX package lives in a read-only directory, so that runtime write path needs to move to a writable per-user location before the app is actually submitted to Partner Center.
