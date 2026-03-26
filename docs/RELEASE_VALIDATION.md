# Release Validation

The current release-validation baseline has two layers.

## 1. Browser smoke

Implemented by `tests/shared/browser_smoke_tests.cpp`.

Current coverage:

- real HTTP page serving for `/host`, `/view`, `/admin/`, `/assets/common.js`, and `/api/status`
- real WS signaling for `host.register`, `room.join`, `peer.joined`, `webrtc.offer`, `webrtc.answer`, and `session.end`
- host-token non-leakage during WebRTC relay

Run it with:

```powershell
.\scripts\windows\browser_smoke.ps1 -Config Release
```

## 2. Desktop release validation

Implemented by `scripts/windows/validate_release.ps1`.

Current coverage:

- validates the desktop payload layout
- smoke-tests the bundled `ViewMeshServer.exe` from the desktop output directory
- launch-tests `ViewMesh.exe` and fails if it exits immediately

Run it with:

```powershell
.\scripts\windows\validate_release.ps1 -Config Release
```

## Recommended Windows flow

Build and validate everything:

```powershell
.\scripts\build.ps1 -Config Release -Target all -Clean
```

Then run explicit checks when needed:

```powershell
.\scripts\windows\browser_smoke.ps1 -Config Release
.\scripts\windows\validate_release.ps1 -Config Release
```

## Current limitations

- browser smoke is a real HTTP / WS client test, not full Chromium/Edge UI automation
- desktop release validation currently checks startup survival and bundled server readiness, but it does not yet validate user-driven UI flows such as bundle export, hotspot actions, or embedded WebView rendering on a real Windows desktop
- field validation is still required on real Windows machines for WebView2/runtime behavior, firewall prompts, and multi-adapter environments
