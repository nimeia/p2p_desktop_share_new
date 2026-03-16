# Development Plan

This plan is organized around the unfinished feature list and aims to move the project from MVP to a releasable internal tool.

## Phase 1: Runtime Reliability

Goal:

- make the generated outputs runnable without manual dependency repair

Work:

- continue certificate/trust UX work now that generation is handled in-process through platform providers
- formalize runtime DLL copying for Debug and Release
- verify desktop output directory contains all required server/runtime assets
- add a basic startup smoke check for server launch
- baseline Windows stage/zip packaging plus install/upgrade/uninstall scripts
- baseline Windows WebView2 Runtime / certificate bootstrap helpers

Exit criteria:

- fresh build output can launch server on a clean dev machine
- desktop app can spawn the server without missing DLL/tool errors

## Phase 2: Desktop Operator Experience

Goal:

- reduce operator confusion during local sharing setup

Work:

- improve hotspot/manual fallback guidance
- improve multi-adapter IP selection messaging
  status: partially done; the desktop shell now ranks active IPv4 candidates, publishes per-candidate LAN probe results, and can export a remote-device probe guide.
- add clearer LAN/firewall/port diagnostics
  status: in progress; local port bind checks, local `/health` probing, Windows Firewall readiness, and remote-device probe orchestration are now wired into desktop self-check/exported diagnostics.
- harden desktop self-check output and action suggestions
  status: in progress; exported self-check now prioritizes port, health, and embedded-host runtime failures
- make WebView2 dependency and failure states explicit in UI
  status: in progress; desktop UI now surfaces WebView status and reports SDK/runtime/controller failure states

Exit criteria:

- operator can understand why sharing is blocked without reading logs
- exported diagnostics point to the top failure cause

## Phase 3: Session And Media Hardening

Goal:

- make real sharing sessions more stable under normal local-network churn

Work:

- add signaling reconnect/recovery rules
- improve viewer recovery after host-side renegotiation or session reset
- improve ICE failure handling and cleanup
- add quality presets and basic media/session telemetry
- run multi-viewer validation on realistic hardware/network setups

Exit criteria:

- session survives common viewer join/leave churn
- operator can distinguish setup failure from media-path failure

## Phase 4: Security And Access Controls

Goal:

- tighten the trust and room-access model without overcomplicating the MVP

Work:

- review host token lifecycle
- decide on optional viewer join token/password
- document certificate trust expectations
- add basic session audit logging

Exit criteria:

- room ownership and join behavior are documented and enforced
- local trust decisions are explicit rather than accidental

## Phase 5: Test And Release Discipline

Goal:

- make changes safer and releases repeatable

Work:

- add unit tests for protocol/util/network helpers
- add integration tests for server routes and signaling
- add browser smoke tests for host/viewer handshake
  status: baseline done; `tests/shared/browser_smoke_tests.cpp` now validates HTTPS host/view pages plus WSS signaling handshake.
- add build/release checklist
- add CI for build + smoke validation
- add desktop release validation for the packaged desktop payload
  status: baseline done; `scripts/windows/validate_release.ps1` now validates copied runtime payload and startup survival.

Exit criteria:

- core regressions are caught automatically
- release build process is documented and repeatable

## Suggested Execution Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 5
5. Phase 4

Reasoning:

- packaging/runtime issues currently block the most basic use.
- operator UX and diagnostics are the next highest leverage.
- session hardening matters after the product reliably starts.
- tests should arrive before the codebase grows much further.
- security hardening should be done with the final runtime/UX model in mind.


## Cross-platform runtime extraction

Goal:

- separate reusable server runtime concerns from Windows-only host orchestration

Current status:

- Iter 2 complete: `lan_screenshare_server` now depends on provider interfaces (`ICertProvider`, `INetworkService`) instead of directly calling `CertManager` / `NetworkManager`
- Iter 4 complete: shared endpoint-selection rules now live in `src/core/network/endpoint_selection.*`, and `NetworkManager` is now a compatibility shim over platform probe/action helpers.
- Windows provider implementations currently wrap the existing managers
- POSIX provider implementations now give the CLI a first portable path on Linux/macOS
- Iter 3 complete: certificate types/SAN parsing/inspection are now shared, and self-signed generation moved into the platform provider layer
- Iter 9 complete: desktop host refresh probing now flows through `host_runtime_coordinator` + `host_runtime_refresh_pipeline`.
- Iter 10 complete: desktop host start/stop/open/export user actions now flow through `host_action_coordinator`.
- Iter 11 complete: desktop host session/config editing and admin/backend sync now flow through `host_session_coordinator`.
- Iter 15 complete: native setup-form synchronization, dirty/pending-apply policy, and setup-page button labels now flow through `desktop_edit_session_presenter`.
- Iter 16 complete: tray/menu/balloon/status-tip shell chrome state now flows through `shell_chrome_presenter`, keeping shell-chrome copy and tray command routing out of `MainWindow.cpp`.


- Iter 18 added a shell bridge presenter / message adapter layer so admin-shell snapshot serialization and host/admin inbound message parsing no longer live directly inside MainWindow/AdminBackend.

- Iter 23 complete: host shell startup/show/restore/close/destroy flow now routes through `host_shell_lifecycle_coordinator`, further reducing Win32-specific lifecycle branching inside `MainWindow.cpp`.
