# Task

## Cross-platform runtime extraction

- [x] Iter 1: establish CMake/runtime layering outline for a future `apps/server_cli` + `src/platform/*` split.
- [x] Iter 2: move server CLI onto provider interfaces so certificate and network resolution are no longer wired directly to `CertManager` / `NetworkManager`.
- [x] Iter 3: split `CertManager` into shared inspection/types + per-platform generation providers.
- [x] Iter 4: split `NetworkManager` into shared endpoint-selection rules + per-platform probes/actions.
- [x] Iter 5: move desktop orchestration logic out of `MainWindow.cpp` into a reusable runtime controller layer.
- [x] Iter 6: move share bundle / diagnostics export out of `MainWindow.cpp` into reusable runtime services.
- [x] Iter 7: centralize repeated desktop session/health assembly into `desktop_runtime_snapshot`.
- [x] Iter 8: route remaining platform-specific quick-fix / system-action dispatch through a platform service facade.
- [x] Iter 9: extract desktop host refresh probing into a host runtime coordinator + refresh pipeline.
- [x] Iter 10: route desktop host start/stop/open/export user actions through a host action coordinator.
- [x] Iter 11: centralize desktop host session/config editing + admin/backend sync through a host session coordinator.
- [x] Iter 12: assemble admin snapshot / dashboard cards / settings read-only fields through a shared admin view-model assembler.
- [x] Iter 13: assemble diagnostics / monitor / shell fallback read-only views through shared diagnostics + shell-state assemblers.
- [x] Iter 14: route native page commands and button-enable policy through a desktop shell presenter / command policy layer.
- [x] Iter 15: extract native form synchronization / edit-dirty state / pending-apply policy into a desktop edit session presenter.


## Iter 12 status

- Completed: `src/core/runtime/admin_view_model_assembler.*` now centralizes admin snapshot shaping, dashboard card text assembly, settings page read-only card assembly, and dashboard suggestion generation.
- `MainWindow.cpp` now builds a shared `AdminViewModelInput` and uses shared view-model assembly for admin snapshot shaping, `RefreshDashboard()`, and `RefreshSettingsPage()` instead of duplicating those derived fields inline.
- Shared smoke coverage now includes `tests/shared/admin_view_model_assembler_tests.cpp`.


## Iter 13 status

- Completed: `src/core/runtime/diagnostics_view_model_assembler.*` now centralizes monitor page metric/detail assembly, diagnostics page checklist/export/file text assembly, and HTML-shell fallback messaging/state.
- `MainWindow.cpp` now consumes shared view-models for `RefreshMonitorPage()`, `RefreshDiagnosticsPage()`, and `RefreshShellFallback()` instead of rebuilding those strings inline.
- Shared smoke coverage now includes `tests/shared/diagnostics_view_model_assembler_tests.cpp`.


## Iter 14 status

- Completed: `src/core/runtime/desktop_shell_presenter.*` now centralizes native desktop button-id routing and button-enable policy shaping for start/stop, hotspot, dashboard suggestion buttons, and adapter-selection buttons.
- `MainWindow.cpp` now resolves native-page commands through the shared shell presenter and applies presenter-driven button policies instead of keeping those routing and enable rules inline.
- Shared smoke coverage now includes `tests/shared/desktop_shell_presenter_tests.cpp`.


## Iter 15 status

- Completed: `src/core/runtime/desktop_edit_session_presenter.*` now centralizes setup-page form draft parsing, dirty/pending-apply state, session-summary derivation for pending edits, and edit-driven button labels/policies.
- `MainWindow.cpp` now treats bind/port/room/token controls as a form draft, applies templates into the draft controls, and only commits those values back into the live session model when an action such as Start consumes them.
- Shared smoke coverage now includes `tests/shared/desktop_edit_session_presenter_tests.cpp`.

## Iter 16 status
- [x] Extract tray/menu/balloon/status-tip shell chrome state into a shared `shell_chrome_presenter`, including tray command routing, tray icon/menu view-models, and native status text assembly.


## Iter 17 status

- Completed: `src/core/runtime/desktop_layout_presenter.*` now centralizes webview surface-mode decisions, native page visibility, and shell-fallback/webview resize geometry.
- `MainWindow.cpp` now routes page visibility through shared presenter state, and `UpdatePageVisibility()` now actively shows/hides grouped native controls instead of acting as a placeholder.
- Shared smoke coverage now includes `tests/shared/desktop_layout_presenter_tests.cpp`.

- [x] Iter 18: shell bridge presenter / message adapter layer

- [x] Iter 19: add admin shell coordinator to centralize AdminBackend command dispatch and MainWindow admin hooks.
- [x] Iter 20: add admin shell runtime publisher to centralize snapshot build/publish trigger flow.
- [x] Iter 21: add host observability coordinator to centralize host-page status/log ingest, poll status aggregation, and native log/timeline filtering.
- [x] Iter 22: add host runtime scheduler to centralize timer interval, UI refresh cadence, and poll tick dispatch policy.

## Iter 20 status

- Completed: `src/core/runtime/admin_shell_runtime_publisher.*` now centralizes admin-shell snapshot-state mapping, snapshot publish trigger policy, and JSON event publishing hooks.
- `AdminBackend` now only handles inbound shell messages, while `MainWindow.cpp` publishes runtime snapshots through the shared publisher instead of rebuilding/publishing them inline.
- Shared smoke coverage now includes `tests/shared/admin_shell_runtime_publisher_tests.cpp`.


## Iter 21 status

- Completed: `src/core/runtime/host_observability_coordinator.*` now centralizes host-page status/log ingest, native log/timeline aggregation, poll result normalization, and diagnostics log filtering.
- `MainWindow.cpp` now routes `AppendLog`, `AddTimelineEvent`, host-page status/log handling, and `/api/status` poll aggregation through the shared observability coordinator instead of mutating these states inline.
- Shared smoke coverage now includes `tests/shared/host_observability_coordinator_tests.cpp`.


## Iter 22 status

- Completed: `src/core/runtime/host_runtime_scheduler.*` now centralizes runtime tick interval defaults plus the timer-tick policy for UI refresh and `/api/status` poll dispatch.
- `MainWindow.cpp` now routes `WM_TIMER` through `HandleRuntimeTick()` and the shared scheduler instead of hard-wiring `UpdateUiState()` and `KickPoll()` directly in the window procedure.
- Shared smoke coverage now includes `tests/shared/host_runtime_scheduler_tests.cpp`.

- [x] Iter 23: add host shell lifecycle coordinator to centralize desktop host startup/show/restore/close/destroy shell flow.
- [x] Iter 24: audit MainWindow.cpp remaining responsibilities and capture the final worthwhile extraction list.
- [x] Iter 25: split `OnCreate()` Win32 control-tree construction into `NativeControlFactory` + `DesktopHostPageBuilders`.

## Iter 23 status

- Completed: `src/core/runtime/host_shell_lifecycle_coordinator.*` now centralizes host shell startup, show/restore, minimize-to-tray, close, tray-exit, and destroy lifecycle plans.
- `MainWindow.cpp` now routes `Show()`, `RestoreFromTray()`, `MinimizeToTray()`, `WM_CLOSE`, tray Exit, and `OnDestroy()` through the shared lifecycle coordinator instead of duplicating those shell-flow decisions inline.
- Shared smoke coverage now includes `tests/shared/host_shell_lifecycle_coordinator_tests.cpp`.


## Iter 24 status

- Completed: added `docs/MAINWINDOW_REMAINING_RESPONSIBILITIES.md` to classify the remaining `MainWindow.cpp` responsibilities into shared-runtime-done, Win32-shell-local, and final optional readability extractions.
- Conclusion: the highest remaining payoff is no longer another shared runtime split; it is a Win32-only control/page builder extraction for `OnCreate()`, followed by an optional WebView shell adapter and shell effect executor split.
- Recommendation: after one or two shell-local readability iterations, freeze `MainWindow.cpp` as the Windows shell façade and move effort to Linux/macOS host shells.

## Iter 25 status

- Completed: added `src/desktop_host/NativeControlFactory.*` to centralize the repetitive `CreateWindowW/CreateWindowExW` shell helpers used by native pages.
- Completed: added `src/desktop_host/DesktopHostPageBuilders.*` to build navigation, dashboard, setup, network, sharing, monitor, diagnostics, settings, and shell-fallback controls outside `MainWindow.cpp`.
- `MainWindow::OnCreate()` is now reduced to server/backend bootstrap, default path/session initialization, `DesktopHostPageBuilders::BuildAll(*this)`, and startup lifecycle handoff.


## Iter 26 status

- Completed: added `src/desktop_host/WebViewShellAdapter.*` to isolate embedded WebView2 initialization hooks, host/admin navigation planning, and current-surface restore behavior out of `MainWindow.cpp`.
- `MainWindow.cpp` now builds WebView shell state/context/hooks and routes lifecycle ensure/restore plus native retry/navigation flows through the adapter instead of inlining `Initialize/Navigate/file-url` handling.
- This slice is Windows-shell-only; no new shared runtime policy was introduced.


## Iter 27 status

- [x] Iter 27: split Win32 command/lifecycle effect application into a `ShellEffectExecutor`.
- Completed: `src/desktop_host/ShellEffectExecutor.*` now centralizes lifecycle plan application, desktop/tray shell command execution, and tray icon/menu Win32 effect handling.
- `MainWindow.cpp` now treats these paths as thin delegating wrappers instead of keeping the Win32 effect application inline.

- [x] Add shared native-shell debounce / notification pipeline so Linux tray and macOS menu-bar can consume stable health, viewer-count, and unexpected-exit signals.


- [x] Add native Linux/macOS shell entry baselines that consume `native_shell_status_tracker` and `ShowNotification(...)`.
- Completed: `apps/linux_tray/main.cpp` now stands up a real AppIndicator-style shell path with periodic live polling, while `apps/macos_menubar/main.mm` mirrors the same shared tracker/controller pipeline through a native Cocoa menu-bar entry.
- Completed: `src/host_shell/native_shell_live_poller.*`, `native_shell_runtime_loop.*`, and `native_shell_action_controller.*` now centralize live poll, debounced status/notification dispatch, and Dashboard / Viewer / Diagnostics actions for future Linux/macOS shell iterations.


- [x] Tighten native Linux/macOS shell UX so Linux tray menu enable/disable policy follows stable health/viewer state and macOS menu actions refresh immediately with native notification feedback.
- Completed: `apps/linux_tray/main.cpp` now disables/enables Dashboard / Viewer / Diagnostics entries from debounced runtime state, updates viewer labels with stable viewer counts, and raises action-result notifications through `ShowNotification(...)`.
- Completed: `apps/macos_menubar/main.mm` now refreshes immediately after every menu action and routes action-result notifications through a Cocoa-native system-actions provider on macOS.


- [x] Deepen `native_shell_action_controller` so Linux/macOS shell actions wait for live readiness after start/stop, validate dashboard refresh against live probes, and reveal exported diagnostics automatically.
- Completed: `src/host_shell/native_shell_action_controller.*` now owns managed POSIX start/stop plus live wait, `RefreshDashboard(...)`, and diagnostics reveal policy through `DiagnosticsExportRevealMode`.
- Completed: `tests/shared/native_shell_action_controller_tests.cpp` now verifies refresh validation, managed start/stop closure, and export reveal behavior.


## Incremental update (2026-03-16, native shell action wiring)

- `apps/linux_tray/main.cpp` now routes tray menu actions through the deeper controller APIs: `RefreshDashboard(...)`, `StartServer(...)`, and `StopServer(...)`, and it accepts `--server-executable/--server-arg` so the native shell can actually launch a managed service process.
- `apps/macos_menubar/main.mm` now mirrors the same controller wiring for Dashboard refresh and managed start/stop, so both native shell entry points share the same live-probe wait / refresh validation semantics instead of keeping legacy shell-local action behavior.

## Incremental update (2026-03-16, firewall / remote reachability diagnostics)

- Added shared `network_diagnostics_policy.*` so dashboard, diagnostics, exported self-check, and shell-bridge snapshot all report a consistent Firewall Inbound Path + Remote Viewer Path readiness state.
- `MainWindow.cpp` now probes Windows Firewall readiness for the running server executable / TCP port, exposes Open Firewall Settings and Run Network Diagnostics actions to the admin shell, and writes the extra readiness state into the desktop runtime snapshot.
- Added `scripts/windows/Run-NetworkDiagnostics.ps1` plus shared smoke coverage in `tests/shared/network_diagnostics_policy_tests.cpp`, and expanded admin-shell/snapshot tests to cover the new actions and payload fields.


## Incremental update (2026-03-16, multi-adapter remote probe orchestration)

- Added shared `remote_probe_orchestrator` planning logic so the desktop shell and admin/web UI can describe which LAN candidate should be tested first and what the operator should do next.
- `MainWindow.cpp` now probes up to four active IPv4 candidates, keeps per-candidate `/health` results, and can export a remote probe guide for a second device.
- Admin shell snapshots now publish `remoteProbeLabel`, `remoteProbeAction`, and per-candidate probe detail so HTML/embedded shells can offer concrete follow-up actions.
- Shared smoke coverage now includes `tests/shared/remote_probe_orchestrator_tests.cpp`.


## Incremental update (2026-03-16, browser smoke / desktop release validation)

- Added `tests/shared/browser_smoke_tests.cpp` so the shared test suite now performs a real HTTPS + WSS smoke pass over `/host`, `/view`, `/assets/common.js`, `/api/status`, `host.register`, `room.join`, `peer.joined`, `webrtc.offer`, `webrtc.answer`, and `session.end`.
- Tightened `ws_session_handlers.cpp` so host-side WebRTC forwarding strips the host `token` before relaying payloads to viewers.
- Added `scripts/windows/browser_smoke.ps1`, `scripts/windows/validate_release.ps1`, and extended `scripts/windows/smoke_server.ps1`/`build.ps1` so Windows builds can automatically run browser smoke and desktop release validation.


## Release readiness status

- [x] RR-7: Windows packaging / installer / upgrade / uninstall baseline
- [x] RR-8: WebView2 runtime policy + certificate bootstrap guidance baseline
- Notes: package stage/zip now lives under `out/package/windows/`, install/uninstall scripts are in `scripts/windows/`, WebView2 Runtime checks now use the documented EdgeUpdate registry locations, and local certificate bypass is restricted to loopback/private-LAN URLs.
