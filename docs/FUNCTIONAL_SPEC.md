# LAN Screen Share Functional Spec

## Scope

This document describes the current MVP behavior implemented in the repository as of 2026-03-10.

Product scope:

- Windows desktop host
- browser-based viewers on the same local network
- local HTTPS/WSS signaling and static hosting
- screen viewing only
- no remote control
- no public internet traversal

Important implementation note:

- the current desktop host is a Win32 shell with optional embedded WebView2
- it is not yet a finished Windows App SDK / WinUI-style product shell

## Roles

- Host: the desktop operator who starts the local service and begins screen sharing
- Viewer: any browser client that opens the viewer URL and watches the shared screen

## Implemented Host Features

### Desktop Startup And Main Window

The desktop host currently provides a single operator window with:

- current host IPv4 display
- bind address, port, room, and token fields
- server start/stop controls
- hotspot controls
- WebView2 host-area status
- room/viewer counters
- share info summary
- diagnostic summary
- suggested next actions
- log output

### Network And Hotspot

Implemented now:

- active host IPv4 detection
- Wi-Fi adapter capability probing
- hotspot capability probing
- Wi-Fi Direct API capability probing
- best-effort hotspot start/stop/query on supported Windows setups
- fallback launch into Windows Mobile Hotspot settings
- fallback launch into Windows connected-devices pairing UI for Wi-Fi Direct

Partially implemented:

- multiple active adapter detection and recommendation
- LAN endpoint self-checks

Not implemented yet:

- full end-to-end Wi-Fi Direct session automation
- firewall diagnostics
- remote-device reachability diagnostics

### Local Service Lifecycle

Implemented now:

- desktop host can start `lan_screenshare_server.exe`
- desktop host can stop the server process
- desktop host polls `/api/status`
- desktop host reports running/stopped state
- desktop host surfaces room/viewer counters

### Embedded Host Page

Implemented now:

- desktop host navigates the host page into embedded WebView2 when available
- host page can also be opened in an external browser
- WebView2 status is surfaced in desktop UI and diagnostics
- self-signed local certificate errors are allowed inside WebView2 for the MVP flow
- host-page status messages are sent back into the desktop shell

Known limitation:

- WebView2 dependency handling is still not productized

### Share Handoff And Export

Implemented now:

- open host URL
- open viewer URL
- copy viewer URL
- generate/export local share artifacts
- open bundle folder

Generated files currently include:

- `share_card.html`
- `share_wizard.html`
- `share_bundle.json`
- `share_status.js`
- `share_diagnostics.txt`
- `desktop_self_check.html`
- `desktop_self_check.txt`
- `viewer_url.txt`
- `hotspot_credentials.txt`
- `share_readme.txt`

### Desktop Self-Check And Diagnostics

Implemented now:

- run self-check from the desktop shell
- re-run checks without starting a new share flow
- open diagnostics text report
- exported issue list with severity/category metadata
- exported issue list with suggested action text
- main-window diagnostic summary
- main-window operator-first-actions summary

## Implemented Viewer Features

Implemented now:

- open viewer URL in a browser
- connect to local WSS signaling
- join room
- receive host offer/ICE messages
- return answer/ICE messages
- recvonly playback of the shared screen
- explicit session-ended handling

Current viewer scope:

- video only
- no upstream capture
- no remote control
- no viewer auth/password flow

## Implemented Service Features

Implemented now:

- HTTPS static file serving for `/host`, `/view`, and `/assets/*`
- health endpoint at `/health`
- status endpoint at `/api/status`
- WebSocket signaling at `/ws`
- one-host/multi-viewer room model
- offer/answer/ICE forwarding
- viewer limit enforcement
- serialized WebSocket send path
- explicit `session.end` to `session.ended` flow

## Acceptance Snapshot

The following behavior is implemented in code today:

- desktop host builds
- local server builds
- desktop host can start/stop the server
- local `/health` endpoint works
- host page can start screen capture through browser/WebView2
- viewer page can receive and play the shared stream
- share bundle pages and diagnostics can be exported locally

The following target behavior is still incomplete:

- robust first-run packaging
- automatic certificate/bootstrap flow without external tool assumptions
- release-safe WebView2 dependency story
- fully guided Wi-Fi Direct workflow
- reconnect/recovery hardening
- multi-viewer validation at scale
- automated tests and CI

## Non-Goals In Current MVP

- internet sharing
- TURN/STUN traversal
- remote control
- file transfer
- viewer-side media capture
- advanced access control

## Source Of Truth

When this file and the code diverge, treat the implementation-truth set below as authoritative:

- `README.md`
- `docs/DEVELOPMENT.md`
- `docs/UNFINISHED_FEATURES.md`
- `docs/PROGRESS_WIP.md`
