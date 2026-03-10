# LAN Screen Share

LAN Screen Share is a Windows-first local screen sharing MVP:

- Desktop host application: currently a Win32 desktop shell with embedded WebView2 support in `desktop_host/`
- Local signaling and static file server: C++20 + Boost.Asio/Beast + OpenSSL
- Browser media path: Host/Viewer pages use WebRTC in the browser/WebView2
- Local-network modes: same LAN/Wi-Fi first, with Windows hotspot helpers and Wi-Fi Direct pairing guidance

The current repository is no longer just a scaffold. It already contains a buildable desktop host, a buildable HTTPS/WSS server, working host/viewer pages, exportable offline share artifacts, and desktop-side self-check tooling.

## Current Status

Implemented today:

- HTTPS static server with `/host`, `/view`, `/assets/*`, `/health`, `/api/status`
- WSS signaling server at `/ws`
- Room model with one host and multiple viewers
- WebRTC offer/answer/ICE forwarding
- Host page screen capture via `getDisplayMedia`
- Viewer page recvonly playback
- Desktop host shell that can:
  - manage the local server process
  - inspect current network state
  - probe hotspot / Wi-Fi capabilities
  - start/stop hotspot on supported Windows setups
  - open Windows Mobile Hotspot settings and Wi-Fi Direct pairing settings as fallback paths
  - embed the host page in WebView2 when the runtime is available
  - surface WebView2 status in the main UI and self-check output
  - generate local share bundle files
  - render/export local QR codes
  - run desktop self-check and export diagnostics
  - show operator-focused diagnostics summary and suggested next actions
- Exported share bundle files:
  - `share_card.html`
  - `share_wizard.html`
  - `share_bundle.json`
  - `share_status.js`
  - `share_diagnostics.txt`
  - `desktop_self_check.html`
  - `desktop_self_check.txt`
- Build scripts for server + desktop app

Not finished yet:

- robust packaging/installer flow
- automatic runtime bootstrap for `openssl.exe`
- production-ready WebView2 dependency handling
- end-to-end Wi-Fi Direct workflow automation
- reconnect/resume and broader stability work
- automated tests and CI
- optional future migration from the current Win32 shell to a Windows App SDK / WinUI-style product shell

See:

- [Development](C:\Users\huang\Downloads\lan_work_p14_unified_issue_actions_20260309\docs\DEVELOPMENT.md)
- [Unfinished Features](C:\Users\huang\Downloads\lan_work_p14_unified_issue_actions_20260309\docs\UNFINISHED_FEATURES.md)
- [Development Plan](C:\Users\huang\Downloads\lan_work_p14_unified_issue_actions_20260309\docs\DEVELOPMENT_PLAN.md)
- [Signaling Protocol](C:\Users\huang\Downloads\lan_work_p14_unified_issue_actions_20260309\docs\SIGNALING_PROTOCOL.md)

## Repository Layout

- `src/`
  - core server, cert, protocol, util, and Windows network helpers
- `www/`
  - `host.html`, `viewer.html`, and JS assets
- `desktop_host/`
  - desktop host app and build notes
- `scripts/`
  - build and certificate helper scripts
- `out/`
  - local build outputs, logs, certs, generated artifacts

## Build

Recommended shell: Visual Studio 2022 Developer PowerShell

Prerequisites:

- Visual Studio 2022 with C++ desktop workload
- CMake 3.24+
- vcpkg

Build everything:

```powershell
.\scripts\build.ps1 -Config Debug -Target all -Clean
```

Build server only:

```powershell
.\scripts\build.ps1 -Config Debug -Target server -Clean
```

Useful outputs:

- Server: `out/bin/Debug/lan_screenshare_server.exe`
- Desktop app: `desktop_host/LanScreenShareHostApp/bin/x64/Debug/LanScreenShareHostApp.exe`
- Build logs: `out/logs/build_*.log`

Server smoke test:

```powershell
.\scripts\windows\smoke_server.ps1 -Config Debug
```

## Runtime Notes

- The server depends on vcpkg runtime DLLs. The build script now copies them into the output directory.
- The build script now also validates the server output layout and runs a smoke check by default for `-Target server`.
- The server still relies on an OpenSSL executable for certificate generation, but the build output now carries a colocated tool path when available.
- The desktop app can run without build-time WebView2 headers, but embedded host-page functionality is degraded in that environment.
- The embedded host page currently allows self-signed certificate errors inside WebView2 for the local MVP flow.
- The desktop shell refreshes exported files in `out/share_bundle/` so the local share pages and diagnostics stay aligned with the current desktop snapshot.

## Suggested Next Work

The most valuable next steps are:

1. finish runtime/bootstrap packaging
2. make WebView2 and certificate flows production-safe
3. harden reconnect/recovery and hotspot/manual-network UX
4. add automated tests and release validation
