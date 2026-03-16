# LAN Screen Share Development Notes

## Goal

Build a Windows desktop host that can share a screen to browsers on the same local network, with minimal operator friction.

Primary constraints:

- local network only
- browser-based viewer
- host-driven sharing flow
- Windows desktop as the main operator environment

## What Is Implemented

### 1. C++ Local Service

The C++ backend already provides:

- HTTPS static file serving
- WSS signaling
- room/session tracking
- host/viewer message forwarding
- JSON-based status endpoint
- certificate loading

Implemented routes:

- `GET /host`
- `GET /view`
- `GET /assets/*`
- `GET /health`
- `GET /api/status`
- `GET /ws` via WebSocket upgrade

### 2. Browser Sharing Flow

The web client side already supports:

- host registration
- viewer room join
- screen capture on host using `getDisplayMedia`
- host-to-many viewer mesh signaling
- viewer recvonly playback
- renegotiation when viewers join after sharing starts
- session end notification

Current media scope:

- video only
- no viewer upstream media
- no remote control

### 3. Windows Desktop Host

The desktop app already includes:

- server process start/stop
- local status and log display
- network mode and host IP inspection
- hotspot capability probing
- hotspot start/stop/query on supported Windows setups
- fallback entry points into Windows Mobile Hotspot settings and Wi-Fi Direct pairing settings
- live room/viewer polling from `/api/status`
- embedded host-page navigation through WebView2 when available
- WebView2 status reporting in the main window and exported diagnostics
- QR/share artifact generation
- desktop self-check and diagnostics export
- copy/open actions for host URL, viewer URL, and generated files
- operator-facing diagnostic summary and suggested remediation actions in the main window

Important implementation note:

- the current desktop shell is a Win32 application with an embedded WebView2 host area
- it is not yet a Windows App SDK / WinUI-style product shell
- the current source tree lives under `src/desktop_host/`

### 4. Share Artifact Export

The app generates a bundle of operator-facing files:

- `share_bundle.json`
- `share_status.js`
- `share_card.html`
- `share_wizard.html`
- `desktop_self_check.html`
- `desktop_self_check.txt`
- `share_diagnostics.txt`
- `viewer_url.txt`
- `hotspot_credentials.txt`
- `share_readme.txt`

These files are designed to let the operator hand off the session even if the desktop app itself is not being watched.

The generated bundle is also used as a shared diagnostics model:

- `share_bundle.json` carries current room/network/runtime state plus self-check results
- `share_wizard.html` and `desktop_self_check.html` consume the same exported issue/action data
- `share_status.js` lets already-open local pages refresh from the latest desktop snapshot

## Architectural Snapshot

```text
Desktop App
  |- starts/stops local C++ server
  |- probes network and hotspot state
  |- polls /api/status for room/viewer counters
  |- exports a shared bundle/self-check snapshot
  |- exports share bundle and diagnostics
  |- optionally embeds host page through WebView2
  |- falls back to external browser/system settings when WebView2 or managed hotspot paths are unavailable

C++ Server
  |- HTTPS static pages
  |- WSS signaling
  |- room/peer state
  |- TLS/certificate loading

Browser Pages
  |- host page captures screen and publishes WebRTC tracks
  |- viewer page receives WebRTC video
```

## Current Limitations

The project is still an MVP. Important missing or weak areas:

- packaging and installer story is now present as a Windows-first baseline, but still needs true field validation and possible MSI/MSIX follow-up
- certificate trust/bootstrap UX now has a Windows helper baseline, but field validation and non-Windows parity are still pending.
- WebView2 productization now has a Windows Evergreen-runtime baseline plus runtime-check helper, but release validation is still incomplete.
- Wi-Fi Direct is still pairing guidance plus capability reporting, not a full automated UX
- hotspot support still depends on Windows hosted-network/manual fallback behavior
- firewall readiness, operator-triggered local network diagnostics, and remote-device probe orchestration are implemented on Windows; follow-up is mostly around true field validation and broader platform parity
- host IP selection is clearer than before because active IPv4 candidates are ranked and probe-scored, but the final UX for automatic switching still needs refinement
- no TURN/STUN or non-LAN transport path
- no reconnect/resume strategy
- shared/unit test coverage exists, but browser/desktop/release validation is still incomplete
- Shared/runtime validation now includes a real HTTPS/WSS browser smoke target and a Windows desktop release-validation script, but full browser UI automation and repeated field validation are still incomplete

## Development Priorities

Priority order should be:

1. make local runtime reliable
2. reduce operator setup friction
3. harden media/session lifecycle
4. add validation and release discipline

Detailed open items and phase planning live in:

- [Unfinished Features](UNFINISHED_FEATURES.md)
- [Development Plan](DEVELOPMENT_PLAN.md)
