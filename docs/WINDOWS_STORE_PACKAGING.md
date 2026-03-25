# Windows Store Packaging

The repository includes an MSIX / Store packaging script:

- `scripts/windows/package_store.ps1`

It reuses the desktop payload layout, writes an `AppxManifest.xml`, generates placeholder Store logo assets when needed, and then produces:

- a `.msix` package
- a `.msixupload` bundle for Partner Center upload unless `-SkipUploadBundle` is set
- a `.msixsym` archive when matching `.pdb` files are present

## Typical usage

Build and pack a Release MSIX:

```powershell
.\scripts\windows\package_store.ps1 `
  -Config Release `
  -IdentityName "YourPartnerCenterIdentity" `
  -Publisher "CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
```

Reuse an existing Release payload, provide Store assets, and sign the package:

```powershell
.\scripts\windows\package_store.ps1 `
  -Config Release `
  -SkipBuild `
  -IdentityName "YourPartnerCenterIdentity" `
  -Publisher "CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" `
  -AssetsDir .\packaging\store-assets `
  -SignPfx .\signing\store_test.pfx `
  -SignPassword "<pfx-password>"
```

## Important parameters

- `-IdentityName`
  - required
  - must match the package identity reserved in Partner Center
- `-Publisher`
  - required
  - must match the Partner Center publisher subject
- `-AssetsDir`
  - optional
  - when omitted, the script generates placeholder PNG assets
- `-SignPfx`
  - optional
  - signs the `.msix` when provided
- `-SkipUploadBundle`
  - optional
  - emits only `.msix` and skips `.msixupload`

## Output layout

Generated files land under:

- `out/package/windows-store/<identity>_<appx-version>_store-x64/`

That directory contains:

- `stage/`
- `AppxManifest.xml`
- `<name>.msix`
- `<name>.msixupload` unless `-SkipUploadBundle`
- `<name>.msixsym` when symbols are found
- `store_package_manifest.json`

The staged runtime includes:

- `LanScreenShareHostApp.exe`
- `lan_screenshare_server.exe`
- `www/`
- `webui/`
- `scripts/windows/Check-WebView2Runtime.ps1`
- `scripts/windows/Run-NetworkDiagnostics.ps1`

## Current blocker before Store submission

The script can build an MSIX container today, but the desktop host still writes diagnostics and share bundle output under `AppDir()\out\...`.

That works for unpackaged builds, but an installed MSIX lives in a read-only directory. Before a real Partner Center submission, writable runtime data needs to move to a per-user location such as LocalAppData and the following paths need to be verified again:

- share bundle export
- diagnostics export
- helper-script output paths
- any other runtime writes that still target the executable directory
