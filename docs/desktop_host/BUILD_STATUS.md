# Desktop Host Build Status

## Current baseline

The current repository baseline provides:

- a scriptable Windows desktop-host build under `out\desktop_host\<Arch>\<Config>\`
- a scriptable local server build under `out\server\<Config>\`
- bundled desktop runtime layout with `lan_screenshare_server.exe`, `www\`, and `webui\`
- Windows validation helpers for server smoke, browser smoke, desktop payload validation, WebView2 runtime checks, and local network diagnostics

## What works in the current build flow

- `scripts/build.ps1` can build `server`, `desktop_host`, or `all`
- `scripts/windows/build.ps1` adds validation and advanced switches
- desktop output is assembled with the runtime files the host expects next to the app
- packaged Windows zip output can be staged through `scripts/windows/package.ps1`
- MSIX container generation exists through `scripts/windows/package_store.ps1`

## Known gaps

- clean-machine validation for packaged install/upgrade/uninstall is still pending
- WebView2 runtime behavior still needs broader operator-environment validation
- release validation is still lighter than true end-to-end UI coverage
- the MSIX flow still has a writable-path blocker because runtime data is written under `AppDir()\out\...`

## Recommended checks

1. build with `.\scripts\build.ps1 -Config Debug -Target all`
2. run `.\scripts\windows\smoke_server.ps1 -Config Debug`
3. run `.\scripts\windows\browser_smoke.ps1 -Config Debug`
4. run `.\scripts\windows\validate_release.ps1 -Config Release`
5. run `.\scripts\windows\Check-WebView2Runtime.ps1` on real operator machines when embedded preview issues are suspected
