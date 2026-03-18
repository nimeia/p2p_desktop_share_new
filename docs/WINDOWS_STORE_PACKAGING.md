# Windows Store Packaging

The repository now includes a Windows Store / MSIX packaging script for the desktop host:

- `scripts/windows/package_store.ps1`

It reuses the existing desktop payload layout, writes an `AppxManifest.xml`, generates placeholder Store logo assets when you do not provide PNGs, then produces:

- a `.msix` package
- a `.msixupload` bundle for Partner Center upload
- a `.msixsym` symbol archive when matching `.pdb` files are present

## Typical usage

Build, stage, and pack a Release MSIX:

```powershell
.\scripts\windows\package_store.ps1 `
  -Config Release `
  -IdentityName "YourPartnerCenterIdentity" `
  -Publisher "CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
```

Reuse an already built Release payload and provide real Store assets plus signing:

```powershell
.\scripts\windows\package_store.ps1 `
  -Config Release `
  -SkipBuild `
  -IdentityName "YourPartnerCenterIdentity" `
  -Publisher "CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" `
  -AssetsDir .\packaging\store-assets `
  -SignPfx .\cert\store_test.pfx `
  -SignPassword "<pfx-password>"
```

## Important parameters

- `-IdentityName`
  - required
  - must match the package identity reserved in Partner Center
- `-Publisher`
  - required
  - must match the Partner Center publisher subject used for the package identity
- `-AssetsDir`
  - optional
  - when omitted, the script generates placeholder PNG assets so the package can still be built
- `-SignPfx`
  - optional
  - if supplied, the script signs the `.msix`; otherwise it leaves the package unsigned
- `-SkipUploadBundle`
  - optional
  - when set, the script only emits `.msix` and skips `.msixupload`

## Output

Generated files land under:

- `out/package/windows-store/<identity>_<appx-version>_store-x64/`

That directory contains:

- `stage/`
- `AppxManifest.xml`
- `<name>.msix`
- `<name>.msixupload` unless `-SkipUploadBundle`
- `<name>.msixsym` when symbols are found
- `store_package_manifest.json`

## Current blocker before Store submission

The script can generate an MSIX package today, but the current desktop host still writes diagnostics and share bundle output under `AppDir()\out\...`.

That works for the unpackaged desktop build, but it is not valid inside an installed MSIX because the package install directory is read-only.

Before a real Partner Center submission, move writable runtime data to a per-user writable location such as LocalAppData and verify:

- share bundle export
- diagnostics export
- helper-script output paths
- any other runtime writes that currently target the executable directory
