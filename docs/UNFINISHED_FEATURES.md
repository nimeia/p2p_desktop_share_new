# Unfinished Features

This list tracks current open work rather than original scaffold ideas.

## 1. Packaging and install validation

- Run clean-machine validation for the Windows install/upgrade/uninstall flow.
- Move writable runtime output away from `AppDir()\out\...` so MSIX installs can become submission-safe.
- Decide how far Windows release packaging should go beyond the current zip/MSIX baselines.

## 2. WebView2 productization

- Field-validate the WebView2 runtime helper flow on real operator machines.
- Improve embedded admin/host recovery when runtime/controller creation fails.
- Tighten the supported story for builds that compile without WebView2 SDK headers.

## 3. Network and sharing UX

- Turn Wi-Fi Direct from capability reporting into a real guided flow.
- Improve hotspot fallback guidance when managed control is unavailable.
- Improve multi-adapter host-IP selection and longer-lived preference handling.
- Continue refining remote-device probe and LAN diagnostics guidance.

## 4. Session reliability

- Add reconnect/retry behavior for signaling and viewer recovery.
- Clarify host restart / session recreation behavior.
- Improve ICE failure handling and operator-visible diagnostics.
- Harden state transitions around start/stop/share-ended timing.

## 5. Native Linux/macOS shell maturity

- Validate Linux tray and macOS menu-bar behavior on real packaged machines.
- Harden managed server start/stop behavior and diagnostics export paths.
- Improve platform-specific notifications and operator UX polish.

## 6. Tests and release validation

- Add broader browser automation beyond the current C++ smoke target.
- Add release checks for bundle export, diagnostics export, and WebView-specific behavior.
- Expand cross-platform CI/runtime coverage for Linux and macOS native shells.

## 7. Documentation and operations

- Keep architecture/build/package docs aligned with the actual implementation.
- Add an operator runbook for common failure cases.
- Publish a known-environment matrix for packaging and runtime validation.
