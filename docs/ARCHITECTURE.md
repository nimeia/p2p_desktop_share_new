# LAN Screen Share Architecture

## Current shape

The repository currently has four main runtime pieces:

1. Windows desktop host shell
2. local C++ HTTP / WS service
3. browser host page
4. browser viewer page

It also now has secondary native-shell entry points for Linux and macOS.

## Runtime topology

```text
Windows Desktop Host (Win32 + optional WebView2)
  |- starts/stops the local service
  |- inspects LAN / hotspot / local status
  |- computes host/viewer URLs
  |- exports share bundle and diagnostics
  |- renders operator guidance

Local C++ Service (Boost.Asio/Beast)
  |- serves /host /view /admin /assets/*
  |- exposes /health and /api/status
  |- accepts /ws signaling
  |- tracks one host + multiple viewers per room

Host Page (browser or embedded WebView2)
  |- calls getDisplayMedia()
  |- publishes WebRTC offers/tracks
  |- reports status/log signals back to the desktop shell

Viewer Page (browser)
  |- joins a room
  |- receives signaling
  |- plays recvonly media

Linux/macOS Native Shells
  |- poll /health and /api/status
  |- expose dashboard/viewer/diagnostics actions
  |- optionally manage a local server process
```

## Responsibility split

### Desktop host shell

The Windows shell currently owns:

- local service lifecycle
- network capability probing
- hotspot and system-settings fallbacks
- WebView2 embedding and runtime reporting
- share bundle export
- diagnostics export and operator-facing health summaries

### Local C++ service

The service currently owns:

- static page and asset hosting
- WebSocket signaling
- room/session tracking
- viewer limit enforcement
- `/health` and `/api/status`

It does not currently provide:

- TURN/STUN traversal
- internet transport
- SFU media routing
- advanced reconnect orchestration

### Browser host/viewer pages

The browser layer intentionally owns media capture and WebRTC:

- `getDisplayMedia()` already exists there
- browser/WebView2 already provides the WebRTC implementation
- the service stays focused on local signaling and asset hosting

### Shared runtime/platform layer

The repository now pushes more behavior through shared modules:

- runtime/presenter/state-shaping code under `src/core/runtime/`
- shared endpoint-selection rules under `src/core/network/`
- platform services under `src/platform/windows/`, `src/platform/posix/`, and `src/platform/macos/`

This keeps `MainWindow.cpp` and the native-shell entry apps from duplicating the same orchestration rules.

## Current constraints

- transport is plain HTTP / WS only
- WebView2 remains optional and fallback-oriented
- Wi-Fi Direct is still guided rather than fully automated
- hotspot handling is best-effort and environment-dependent
- multi-viewer scale is still mesh-based
- packaged-app writable-path cleanup is still required before Store submission

## Historical refactor notes

For iteration-by-iteration extraction history, see:

- `CROSS_PLATFORM_REFACTOR_PLAN.md`
- `MAINWINDOW_REMAINING_RESPONSIBILITIES.md`
- `PROGRESS_WIP.md`

Treat those as historical context. This file only describes the current runtime shape.
