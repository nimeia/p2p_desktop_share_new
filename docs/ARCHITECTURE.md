# LAN Screen Share Architecture

## Current Shape

The repository currently implements a Windows-first local sharing MVP with four main parts:

1. Desktop host shell
2. Local HTTPS/WSS service
3. Browser host page
4. Browser viewer page

The desktop layer is currently a Win32 application with an embedded WebView2 area. It is not yet a Windows App SDK / WinUI-style product shell.

## Runtime Topology

```text
Desktop Host Shell (Win32 + optional WebView2)
  |- probes LAN / Wi-Fi / hotspot capability
  |- starts/stops local server process
  |- selects host IP / bind / room / token
  |- opens host/viewer URLs
  |- exports share bundle and diagnostics
  |- renders operator status and first actions

Local C++ Service (Boost.Asio/Beast + OpenSSL)
  |- serves /host /view /assets/*
  |- exposes /health and /api/status
  |- accepts /ws WebSocket signaling
  |- tracks one host + multiple viewers per room

Host Page (browser or WebView2)
  |- calls getDisplayMedia()
  |- creates WebRTC offers
  |- pushes status/log events back to desktop shell

Viewer Page (browser)
  |- joins room
  |- answers offers
  |- receives recvonly media
```

## Why This Split

The project intentionally keeps media capture and WebRTC inside the browser engine:

- `getDisplayMedia()` is already available there
- browser/WebView2 provides the WebRTC implementation
- the C++ service stays focused on local signaling, static asset hosting, and TLS

This keeps the MVP small and lets the desktop app stay focused on operator workflow, diagnostics, and local environment handling.

## Current Desktop Responsibilities

The desktop shell currently owns:

- local server process lifecycle
- network capability probing
- hotspot start/stop/query on supported systems
- fallback launch into Windows Mobile Hotspot settings
- fallback launch into Windows connected-devices pairing UI for Wi-Fi Direct
- embedded host-page navigation when WebView2 is available
- WebView2 runtime/controller status reporting
- local share bundle export
- desktop self-check generation
- operator-facing diagnostics summary

## Current Server Responsibilities

The C++ local service currently owns:

- HTTPS static file serving
- WebSocket signaling
- room/session tracking
- viewer limit enforcement
- status endpoints for the desktop shell
- TLS certificate loading

It does not currently provide:

- TURN/STUN traversal
- internet transport
- SFU media routing
- advanced recovery orchestration

## Current Browser Responsibilities

Host page:

- registers host identity
- starts screen capture
- creates and renegotiates offers
- stops sharing and emits session end
- reports status back to the desktop shell

Viewer page:

- joins a room
- receives offer/ICE messages
- answers with recvonly playback
- handles explicit session-ended state

## Shared Diagnostics Model

One important implementation detail in the current codebase is that diagnostics are exported once and consumed in multiple places.

The desktop app writes:

- `share_bundle.json`
- `share_status.js`
- `share_card.html`
- `share_wizard.html`
- `desktop_self_check.html`
- `desktop_self_check.txt`
- `share_diagnostics.txt`

The generated pages and reports consume the same exported self-check issue/action model so the main window, wizard, desktop self-check view, and text diagnostics do not drift independently.

## Current Constraints

The architecture is still MVP-grade. Known structural constraints:

- self-signed certificate flow is still operational rather than productized
- WebView2 dependency handling is still fallback-oriented
- Wi-Fi Direct is guided via system UI, not end-to-end automated
- hotspot control is best effort and depends on Windows behavior
- reconnect/recovery behavior is still limited
- multi-viewer scale is still mesh-based and needs more validation
