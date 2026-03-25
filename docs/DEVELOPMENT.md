# LAN Screen Share Development Notes

## Goal

Build a Windows-first desktop host that can share a screen to browsers on the same local network with low operator friction.

Primary constraints:

- local network first
- browser-based host/viewer pages
- Windows desktop as the main operator environment
- Linux tray and macOS menu-bar support as secondary native-shell baselines

## What is implemented

### 1. Local C++ service

The current backend already provides:

- plain HTTP page serving
- WS signaling
- room/session tracking
- JSON status output for the desktop host
- host/viewer URL generation support through the shared runtime/platform layer

Implemented routes include:

- `GET /host`
- `GET /view`
- `GET /admin/`
- `GET /assets/*`
- `GET /health`
- `GET /api/status`
- `GET /ws` via WebSocket upgrade

### 2. Browser sharing flow

The web client side already supports:

- host registration
- viewer room join
- screen capture on host via `getDisplayMedia`
- host-to-many viewer signaling
- viewer recvonly playback
- renegotiation when viewers join after sharing starts
- explicit session-end propagation

### 3. Windows desktop host

The desktop app already includes:

- server process start/stop
- local status and log display
- network mode and host-IP inspection
- hotspot capability probing and supported fallback actions
- live room/viewer polling from `/api/status`
- embedded admin/host navigation through WebView2 when available
- WebView2 runtime status reporting
- QR/share artifact generation
- desktop self-check and diagnostics export
- copy/open actions for host URL, viewer URL, and generated files

### 4. Native Linux/macOS shell baseline

The repository also now includes:

- a Linux tray entry point under `apps/linux_tray/`
- a macOS menu-bar entry point under `apps/macos_menubar/`
- shared native-shell polling/action logic under `src/host_shell/`

These shells are still earlier-stage than the Windows desktop host, but they are real packaged/runtime entry points rather than stubs.

### 5. Share artifact export

The app exports operator-facing files such as:

- `share_bundle.json`
- `share_status.js`
- `share_card.html`
- `share_wizard.html`
- `desktop_self_check.html`
- `desktop_self_check.txt`
- `share_diagnostics.txt`

These outputs are kept in sync with the current desktop snapshot and are intended to let the operator hand off the session even when the desktop window is not being watched directly.

## Current architectural notes

- the current desktop shell is a Win32 application with optional embedded WebView2
- transport is plain HTTP / WS only
- the old local certificate/bootstrap flow is no longer part of the runtime
- shared runtime/presenter modules now own more of the state shaping that used to live directly in `MainWindow.cpp`
- platform-specific network and system actions are routed through `src/platform/*`

## Current limitations

- Windows packaging/install/uninstall now exists, but still needs clean-machine field validation
- WebView2 runtime handling has a baseline helper flow, but still needs broader operator-environment validation
- Wi-Fi Direct is still primarily guided fallback behavior rather than a complete product flow
- hotspot behavior is still dependent on Windows capability and environment
- reconnect/recovery and broader session hardening are still limited
- browser/desktop validation exists, but real hardware and UI coverage are still incomplete

## Current priorities

1. field-validate the packaged Windows install/upgrade/uninstall flow
2. field-validate the WebView2 runtime helper flow on real operator machines
3. harden reconnect/recovery plus hotspot/manual-network UX
4. deepen browser automation and release validation on real hardware

## Related docs

- [ARCHITECTURE.md](ARCHITECTURE.md)
- [UNFINISHED_FEATURES.md](UNFINISHED_FEATURES.md)
- [BUILD_WINDOWS.md](BUILD_WINDOWS.md)
- [RELEASE_VALIDATION.md](RELEASE_VALIDATION.md)
