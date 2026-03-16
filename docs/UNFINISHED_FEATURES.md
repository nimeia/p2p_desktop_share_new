# Unfinished Features

This list is based on the current codebase, not the original scaffold plan.

## 1. Runtime Bootstrap And Packaging

- Improve first-run certificate trust/bootstrap guidance across Windows, Linux, and macOS.
  status: Windows baseline landed; helper scripts now cover WebView2 Runtime checks and local certificate import, but Linux/macOS parity and field validation remain open.
- Replace ad hoc output copying with a stable packaging flow.
  status: Windows baseline landed through `scripts/windows/package.ps1`, `Install-LanScreenShare.ps1`, and `Uninstall-LanScreenShare.ps1`.
- Add release packaging for the desktop app and server dependencies.
  status: Windows baseline landed; broader release automation and signing are still open.
- Validate desktop output layout so the app can be moved to another machine without manual DLL hunting.
  status: desktop release validation + package staging now validate the current payload layout, but clean-machine field validation is still required.

## 2. WebView2 Productization

- Replace the current compile-time fallback path with a clear supported dependency story.
  status: Windows baseline now expects Evergreen WebView2 Runtime and ships a dedicated runtime-check helper, but final support policy and field validation are still open.
- Ensure WebView2 runtime presence is detected and reported cleanly.
  status: baseline done; runtime detection now checks the documented EdgeUpdate registry locations and surfaces helper-driven remediation in the Windows packaging flow.
- Improve embedded host-page lifecycle and error recovery.
- Verify certificate-error handling behavior in embedded WebView2 across target environments.
  status: baseline tightened; certificate bypass is now restricted to loopback/private-LAN URLs, but repeated environment validation is still required.

## 3. Network And Sharing UX

- Turn Wi-Fi Direct from capability reporting into an end-to-end guided flow.
- Improve hotspot fallback guidance when hosted-network control is unavailable.
- Add better LAN diagnostics for firewall, reachability, and port conflicts.
  status: partially done; port conflict checks, local server reachability, Windows Firewall readiness, operator-triggered local network diagnostics, and remote-device probe orchestration are implemented, but they still need Windows field validation and cross-platform parity.
- Clarify host IP selection when multiple active adapters exist.
  status: partially done; the app now ranks active IPv4 candidates, records per-candidate probe results, and can export a remote probe guide, but automatic switching and longer-lived adapter preference are still open.

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
  status: partially done; `tests/shared/browser_smoke_tests.cpp` now validates HTTPS page serving plus WSS host/viewer negotiation, but real browser UI automation is still open.
- Desktop-side smoke tests for build, launch, bundle export, and hotspot state reads.
  status: partially done; `scripts/windows/validate_release.ps1` now validates the desktop payload and startup survival, but bundle export, hotspot state reads, and WebView-specific release checks are still open.

## 8. Documentation And Operations

- Keep architecture and build docs in sync with the actual implementation.
- Add operator-facing runbook for common failure cases.
- Add release checklist and known environment matrix.
