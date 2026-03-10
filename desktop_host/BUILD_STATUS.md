# WinUI/Desktop Build Status

## Current State

The desktop host app is buildable in Debug with the current repository layout.

Verified recently:

- `.\scripts\build.ps1 -Config Debug -Target all -Clean`
- `.\scripts\build.ps1 -Target server`
- `.\scripts\windows\smoke_server.ps1 -Config Debug`
- desktop app build succeeds
- C++ server build succeeds
- runtime Boost/OpenSSL DLLs are copied into server output
- server output layout is validated after build
- server `/health` smoke check passes from built output

## What Works

- desktop executable is produced
- server executable is produced
- server/cert/www assets are copied next to the desktop app
- local share pages and diagnostics bundle generation are implemented

## Known Gaps

- WebView2 build-time dependency handling is still fallback-oriented, not polished packaging
- first-run certificate generation still depends on finding `openssl.exe`
- there is no installer/MSIX/release packaging flow yet
- automated smoke tests for the desktop app do not exist

## Recommended Near-Term Follow-Up

1. package or vendor the certificate generation dependency
2. make WebView2 dependency resolution explicit and release-safe
3. add post-build smoke validation for server start, bundle export, and desktop startup
