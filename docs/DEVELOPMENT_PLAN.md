# Development Plan

This plan is organized around the unfinished feature list and aims to move the project from MVP to a releasable internal tool.

## Phase 1: Runtime Reliability

Goal:

- make the generated outputs runnable without manual dependency repair

Work:

- package certificate-generation dependency or remove external `openssl.exe` requirement
- formalize runtime DLL copying for Debug and Release
- verify desktop output directory contains all required server/runtime assets
- add a basic startup smoke check for server launch

Exit criteria:

- fresh build output can launch server on a clean dev machine
- desktop app can spawn the server without missing DLL/tool errors

## Phase 2: Desktop Operator Experience

Goal:

- reduce operator confusion during local sharing setup

Work:

- improve hotspot/manual fallback guidance
- improve multi-adapter IP selection messaging
- add clearer LAN/firewall/port diagnostics
  status: in progress; local port bind checks and local `/health` probing are now wired into desktop self-check/exported diagnostics
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
- add build/release checklist
- add CI for build + smoke validation

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
