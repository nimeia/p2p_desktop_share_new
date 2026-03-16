# Release Validation Baseline

This repository now has a first release-validation layer for Windows-first internal releases.

## Coverage

The baseline is split into two parts:

1. **Browser smoke**
   - implemented by `tests/shared/browser_smoke_tests.cpp`
   - validates real HTTPS page serving for `/host`, `/view`, `/assets/common.js`, and `/api/status`
   - validates real WSS signaling for `host.register`, `room.join`, `peer.joined`, `webrtc.offer`, `webrtc.answer`, and `session.end`
   - verifies the host token is not forwarded to viewers during WebRTC message relay

2. **Desktop release validation**
   - implemented by `scripts/windows/validate_release.ps1`
   - validates the desktop payload layout
   - smoke-tests the bundled `lan_screenshare_server.exe` from the desktop output directory
   - launch-tests `LanScreenShareHostApp.exe` and fails if it exits immediately

## Recommended Windows flow

Build and validate everything:

```powershell
.\scripts\build.ps1 -Config Release -Target all -Clean
```

Run browser smoke explicitly:

```powershell
.\scripts\windows\browser_smoke.ps1 -Config Release
```

Run desktop release validation explicitly:

```powershell
.\scripts\windows\validate_release.ps1 -Config Release
```

## Current limitations

- Browser smoke is implemented as a real HTTPS/WSS client test, not as a full Chromium/Edge UI automation run.
- Desktop release validation currently checks startup survival and bundled server readiness, but it does not yet verify user-driven UI flows such as bundle export, hotspot actions, or WebView content loading on a real Windows desktop.
- Field validation is still required on real Windows machines for WebView2/runtime behavior, firewall prompts, and multi-adapter environments.
