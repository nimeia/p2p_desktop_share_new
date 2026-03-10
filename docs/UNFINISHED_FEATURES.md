# Unfinished Features

This list is based on the current codebase, not the original scaffold plan.

## 1. Runtime Bootstrap And Packaging

- Bundle or otherwise guarantee `openssl.exe` availability for first-run certificate generation.
- Replace ad hoc output copying with a stable packaging flow.
- Add release packaging for the desktop app and server dependencies.
- Validate desktop output layout so the app can be moved to another machine without manual DLL hunting.

## 2. WebView2 Productization

- Replace the current compile-time fallback path with a clear supported dependency story.
- Ensure WebView2 runtime presence is detected and reported cleanly.
  status: partially done; the desktop app now reports WebView status in UI/self-check, but dependency installation and support policy are still not finalized.
- Improve embedded host-page lifecycle and error recovery.
- Verify certificate-error handling behavior in embedded WebView2 across target environments.

## 3. Network And Sharing UX

- Turn Wi-Fi Direct from capability reporting into an end-to-end guided flow.
- Improve hotspot fallback guidance when hosted-network control is unavailable.
- Add better LAN diagnostics for firewall, reachability, and port conflicts.
  status: partially done; port conflict checks and local server reachability checks are implemented, but firewall and remote-device reachability are still missing.
- Clarify host IP selection when multiple active adapters exist.

## 4. WebRTC Session Reliability

- Add reconnect/retry behavior for signaling and viewer recovery.
- Handle host restart / session recreation more explicitly.
- Add better state transitions when share starts before/after viewers join.
- Add more robust ICE failure handling and operator-visible diagnostics.

## 5. Media Features

- Decide whether host audio sharing is in scope and implement it if needed.
- Add configurable quality presets instead of relying on browser defaults.
- Add explicit limits/telemetry for viewer count, bitrate, and degraded states.
- Validate mesh behavior under real multi-viewer load.

## 6. Security And Access Control

- Strengthen host token handling and room lifecycle rules.
- Decide whether viewers need optional join tokens or room passwords.
- Review trust model for self-signed certificates and local export artifacts.
- Add basic audit/logging around session creation and termination.

## 7. Tests And Validation

- Unit tests for protocol/message helpers and network parsing.
- Integration tests for `/health`, `/api/status`, static pages, and WSS signaling.
- Browser-level smoke tests for host/viewer negotiation.
- Desktop-side smoke tests for build, launch, bundle export, and hotspot state reads.

## 8. Documentation And Operations

- Keep architecture and build docs in sync with the actual implementation.
- Add operator-facing runbook for common failure cases.
- Add release checklist and known environment matrix.
