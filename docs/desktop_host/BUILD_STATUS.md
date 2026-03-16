# WinUI/Desktop Build Status

## Current State

The desktop host app is buildable in Debug with the current repository layout.

Verified recently:

- `.\scripts\build.ps1 -Config Debug -Target all -Clean`
- `.\scripts\build.ps1 -Target server`
- `.\scripts\windows\smoke_server.ps1 -Config Debug`
- `.\scripts\windows\browser_smoke.ps1 -Config Debug`
- `.\scripts\windows\validate_release.ps1 -Config Release`
- desktop app build succeeds
- C++ server build succeeds
- runtime Boost/OpenSSL DLLs are copied into server output
- server output layout is validated after build
- server `/health` smoke check passes from built output

## What Works

- desktop executable is produced under `out\desktop_host\<Arch>\<Config>\`
- server executable is produced under `out\server\<Config>\`
- server/cert/www assets are copied next to the desktop app
- local share pages and diagnostics bundle generation are implemented

## Known Gaps

- Windows packaging now has a repeatable stage/zip baseline through `scripts/windows/package.ps1` plus install/upgrade/uninstall scripts, but it still needs clean-machine field validation.
- WebView2 Runtime detection now has a dedicated Windows helper (`Check-WebView2Runtime.ps1`) and the desktop host reports runtime-unavailable more explicitly, but real operator environments still need repeated validation.
- first-run certificate trust/bootstrap now has a dedicated Windows helper (`Trust-LocalCertificate.ps1`) and certificate bypass is restricted to loopback/private-LAN flows, but certificate UX still needs field validation.
- browser-level HTTPS/WSS smoke now exists through `tests/shared/browser_smoke_tests.cpp`, and Windows desktop release validation now exists through `scripts/windows/validate_release.ps1`, but both still need repeated field validation on real Windows hardware

## Recommended Near-Term Follow-Up

1. run clean-machine install/upgrade/uninstall validation for the packaged Windows payload
2. validate the WebView2 Runtime + certificate bootstrap helpers on real operator machines
3. extend post-build validation beyond startup into bundle export, WebView2 runtime checks, and remote-device probe flows on field hardware
