# Cross-platform runtime refactor plan

## Current status

The repository still has a Windows-first desktop host, but the server/runtime path now has both certificate and network seams split out: the standalone CLI talks to providers, certificate inspection/types live in shared cert files, and endpoint-selection rules now live in shared network files.

## Iter 2 delivered

- Added `ICertProvider` and `INetworkService` interfaces under `src/platform/abstraction/`
- Added Windows wrappers under `src/platform/windows/`
- Added POSIX wrappers under `src/platform/posix/`
- Moved the active server CLI entry to `apps/server_cli/main.cpp`
- Updated CMake so the CLI composes shared runtime + abstraction + platform-default providers

## Iter 3 delivered

- Added `src/core/cert/cert_types.h`, `cert_san.*`, and `cert_inspector.*`
- Reduced `CertManager` to a compatibility shim over `CertInspector`
- Moved self-signed certificate generation into `src/platform/common/self_signed_cert_generator.cpp`
- Updated Windows/POSIX cert providers to own generation through the provider layer
- Added a small shared cert smoke test target under `tests/shared/cert_tests.cpp`

## Next cuts

1. keep hotspot/Wi-Fi Direct actions Windows-only, but lift any reusable diagnostics/status shaping into shared runtime services
2. move desktop orchestration out of `MainWindow.cpp`
3. let the future Linux/macOS host depend on the provider/probe layer rather than on `NetworkManager` compatibility shims

## Iter 4 delivered

- Added `src/core/network/network_types.h` and `endpoint_selection.*` for shared endpoint-selection rules
- Reduced `NetworkManager` to a compatibility shim over platform probe/action helpers
- Moved Windows adapter probing into `src/platform/windows/network_probe_win.*`
- Moved Windows hotspot/capability actions into `src/platform/windows/network_actions_win.*`
- Added POSIX probe/action baselines under `src/platform/posix/`
- Updated the provider-based CLI network services to resolve endpoints through shared rules instead of duplicating selection logic
- Added a small shared network smoke test target under `tests/shared/network_tests.cpp`


## Iter 5 status

- Completed: `src/core/runtime/runtime_controller.*` now owns host/viewer URL building, dashboard overall-state computation, handoff-state decisions, and share-info composition.
- `MainWindow.cpp` still owns Win32 controls and platform probing, but no longer owns those orchestration rules directly.


## Iter 6 status

- Completed: `src/core/runtime/share_artifact_service.*` now owns share bundle JSON generation, offline handoff page generation, diagnostics/self-check text export, and artifact file emission.
- `MainWindow::WriteShareArtifacts()` now only gathers session/health/cert state and delegates the actual export work to the shared runtime service.
- Added a shared artifact smoke test target under `tests/shared/share_artifact_service_tests.cpp`.


## Iter 7 status

- Completed: `src/core/runtime/desktop_runtime_snapshot.*` now centralizes repeated desktop session + health + self-check + handoff snapshot assembly.
- `MainWindow.cpp` now consumes a single runtime snapshot builder instead of recomputing those derivations in multiple UI paths.


## Iter 8 status

- Added `src/platform/abstraction/system_actions.h` and `src/platform/abstraction/platform_service_facade.h` to centralize platform-specific quick-fix/system-action dispatch.
- Added Windows and POSIX `system_actions_*` implementations for opening URLs, paths, and system settings entry points.
- Added `src/platform/platform_service_facade.cpp` so desktop code can consume one facade for network actions + platform open actions.
- `MainWindow.cpp` now routes hotspot actions, connected-devices settings, external URL launches, diagnostics opening, and bundle-folder opening through the facade rather than calling `NetworkManager` / `ShellExecuteW` directly.
- Added `tests/shared/platform_service_facade_tests.cpp` as a small facade delegation smoke test.


## Iter 9 status

- Completed: `src/core/runtime/host_runtime_coordinator.*` and `src/platform/host_runtime_refresh_pipeline.*` now centralize desktop network/hotspot probe refresh assembly.
- `MainWindow.cpp` now consumes one refresh pipeline instead of separately probing network info, capabilities, and hotspot state.

## Iter 10 status

- Completed: `src/core/runtime/host_action_coordinator.*` now centralizes desktop start/stop/open/export user action sequencing.
- `MainWindow.cpp` now maps host actions onto a small set of local hooks (`PerformStartServerAction`, `PerformOpenHostPageAction`, `PerformEnsureShareArtifactsAction`, etc.) instead of directly hand-coding those action flows in each UI command path.
- Shared smoke coverage now includes `tests/shared/host_action_coordinator_tests.cpp`.


## Iter 11 status

- Completed: `src/core/runtime/host_session_coordinator.*` now centralizes desktop session/config normalization, room/token generation, and delivery-state reset semantics after session edits.
- `MainWindow.cpp` now routes random room/token generation, admin-shell session apply, and start-time session preparation through the shared session coordinator instead of duplicating that logic inline.
- The admin shell snapshot/runtime publish path now sources editable session/default/rule fields from shared session + admin-shell publisher layers, keeping admin/backend state in sync with the native desktop host.
- Shared smoke coverage now includes `tests/shared/host_session_coordinator_tests.cpp`.


## Iter 12 status

- Completed: `src/core/runtime/admin_view_model_assembler.*` now centralizes admin snapshot shaping, dashboard card text assembly, settings page read-only card assembly, and dashboard suggestion generation.
- `MainWindow.cpp` now builds a shared `AdminViewModelInput` and uses shared view-model / admin-shell publisher assembly for admin snapshot publish, `RefreshDashboard()`, and `RefreshSettingsPage()` instead of duplicating those derived fields inline.
- Shared smoke coverage now includes `tests/shared/admin_view_model_assembler_tests.cpp`.

## Iter 13 status

- Completed: `src/core/runtime/diagnostics_view_model_assembler.*` now centralizes monitor page metric/detail assembly, diagnostics page checklist/export/file text assembly, and HTML-shell fallback messaging/state assembly.
- `MainWindow.cpp` now consumes shared view-models for `RefreshMonitorPage()`, `RefreshDiagnosticsPage()`, and `RefreshShellFallback()` instead of rebuilding those strings inline.
- Shared smoke coverage now includes `tests/shared/diagnostics_view_model_assembler_tests.cpp`.


## Iter 14 status

- Completed: `src/core/runtime/desktop_shell_presenter.*` now centralizes native-page command routing and button-enable policy shaping for the desktop shell.
- Added `src/desktop_host/DesktopCommandIds.h` so `MainWindow.cpp` and the presenter share one command-id source of truth.
- `MainWindow.cpp` now resolves native-page button commands through `ResolveDesktopShellCommand(...)` and applies shared button policies for dashboard, hotspot/start-stop, shell fallback, and network adapter selection.
- Shared smoke coverage now includes `tests/shared/desktop_shell_presenter_tests.cpp`.


## Iter 15 status

- Completed: `src/core/runtime/desktop_edit_session_presenter.*` now centralizes native setup-form draft parsing, dirty/pending-apply detection, and setup-page button/summary policy for edits that have not yet been pushed into the active host session.
- `MainWindow.cpp` now uses the shared edit presenter to treat bind/port/room/token controls as a draft, apply templates into those controls, and only commit those values back into the host session model when a consuming action such as Start runs.
- Shared smoke coverage now includes `tests/shared/desktop_edit_session_presenter_tests.cpp`.


## Iter 16 status
- [x] Extract tray/menu/balloon/status-tip shell chrome state into a shared `shell_chrome_presenter`, including tray command routing, tray icon/menu view-models, and native status text assembly.


## Iter 17 status

- Completed: `src/core/runtime/desktop_layout_presenter.*` now centralizes desktop shell surface-mode selection, native page visibility, and fallback/webview resize geometry.
- `MainWindow.cpp` now uses the shared layout presenter to pick between HTML admin, host preview, and hidden webview surfaces, and to actively toggle grouped native page controls in `UpdatePageVisibility()`.
- Shared smoke coverage now includes `tests/shared/desktop_layout_presenter_tests.cpp`.


- Iter 18 added a shell bridge presenter / message adapter layer so admin-shell snapshot serialization and host/admin inbound message parsing no longer live directly inside MainWindow/AdminBackend.


## Admin shell coordinator
The admin shell bridge now routes parsed admin commands through a shared admin shell coordinator so AdminBackend no longer owns command-to-action mapping directly.


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

## Iter 23 status

- Completed: `src/core/runtime/host_shell_lifecycle_coordinator.*` now centralizes host shell startup, show/restore, minimize-to-tray, close, tray-exit, and destroy lifecycle plans.
- `MainWindow.cpp` now routes `Show()`, `RestoreFromTray()`, `MinimizeToTray()`, `WM_CLOSE`, tray Exit, and `OnDestroy()` through the shared lifecycle coordinator instead of duplicating those shell-flow decisions inline.
- Shared smoke coverage now includes `tests/shared/host_shell_lifecycle_coordinator_tests.cpp`.

- Iter 25: split Win32 native control-tree creation out of `MainWindow::OnCreate()` into `NativeControlFactory` + `DesktopHostPageBuilders`, so the remaining shell work focuses on lifecycle/effect application rather than control construction.

## Iter 26 status

- Completed: `src/desktop_host/WebViewShellAdapter.*` now encapsulates WebView2 shell initialization hooks, admin/host navigation planning, and restore-to-current-surface behavior for the Windows host shell.
- `MainWindow.cpp` now treats embedded web host integration as a shell adapter concern instead of inlining `Initialize`, file-URL building, and navigation state mutations.


## Iter 27 status

- Completed: added `src/desktop_host/ShellEffectExecutor.*` to centralize Win32 lifecycle-plan application, desktop/tray command execution, and tray icon/menu effect handling.
- `MainWindow.cpp` now delegates these shell-local effect paths instead of keeping the Win32 `ShowWindow` / `TrackPopupMenu` / `Shell_NotifyIconW` / clipboard/message-box flow inline.

- Added a shared native-shell debounce + notification layer (`native_shell_alert_coordinator` + `native_shell_status_tracker`) so future Linux/macOS host shells can react to stable runtime changes instead of raw poll jitter.


- Added a lightweight `src/host_shell/` layer plus `apps/linux_tray/main.cpp` and `apps/macos_menubar/main.mm` so Linux/macOS shell entries can consume the shared debounce/notification pipeline without depending on the Win32 shell.
