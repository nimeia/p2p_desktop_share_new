# Windows Script Reference

This folder contains the Windows-specific build, run, validation, diagnostics, and packaging scripts.

## Build

Common wrapper:

```powershell
.\scripts\build.ps1 -Config Debug -Target all
```

Full Windows build entry point:

```powershell
.\scripts\windows\build.ps1 -Config Debug -Target all
```

Use `scripts/windows/build.ps1` when you need Windows-only switches such as:

- `-SkipServerSmoke`
- `-SkipBrowserSmoke`
- `-SkipDesktopValidation`
- `-SkipDesktopHostRestore`
- `-BuildRoot`

Notes:

- `-Generator auto` now resolves through the shared helper logic and prefers Visual Studio when MSBuild is available
- builds are incremental by default
- use `-Clean` only when you need a fresh configure/build

## Run

Run the local server:

```powershell
.\scripts\windows\run_server.ps1 -Config Debug -Port 9443
```

Run the desktop host:

```powershell
.\scripts\windows\run_desktop_host.ps1 -Config Debug
```

Legacy convenience wrapper:

- `run_desktop_host.bat`

## Validation

Smoke-test the bundled server:

```powershell
.\scripts\windows\smoke_server.ps1 -Config Debug
```

Run the C++ browser smoke target:

```powershell
.\scripts\windows\browser_smoke.ps1 -Config Debug
```

Validate the desktop payload and launch behavior:

```powershell
.\scripts\windows\validate_release.ps1 -Config Release
```

Quick desktop launch helper:

```powershell
.\scripts\windows\test_desktop_host.ps1 -Config Debug
```

## Diagnostics

Environment/build-dir diagnostics:

```powershell
.\scripts\windows\doctor.ps1
```

WebView2 runtime check:

```powershell
.\scripts\windows\Check-WebView2Runtime.ps1
```

Local network diagnostics:

```powershell
.\scripts\windows\Run-NetworkDiagnostics.ps1 -HostIp 192.168.1.20 -Port 9443
```

Clean desktop-host build outputs and selected NuGet state:

```powershell
.\scripts\windows\clean_nuget_cache.ps1
.\scripts\windows\clean_nuget_cache.ps1 -IncludeGlobalCache
```

## Packaging

Zip package:

```powershell
.\scripts\windows\package.ps1 -Config Release
```

Store/MSIX package:

```powershell
.\scripts\windows\package_store.ps1 -Config Release -IdentityName "YourPartnerCenterIdentity" -Publisher "CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
```

Install/uninstall helpers are used from the staged package root:

- `Install-LanScreenShare.ps1`
- `Uninstall-LanScreenShare.ps1`

## Output conventions

- server output: `out\server\<Config>\`
- desktop output: `out\desktop_host\<Arch>\<Config>\`
- logs: `out\logs\`
- packaged zip output: `out\package\windows\`
- packaged MSIX output: `out\package\windows-store\`
