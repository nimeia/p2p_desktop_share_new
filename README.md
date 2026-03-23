# LAN Screen Share

LAN Screen Share is a Windows-first local screen sharing MVP:

- Desktop host application: currently a Win32 desktop shell with embedded WebView2 support in `src/desktop_host/`
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

- repeatable Windows packaging/install/upgrade/uninstall baseline (initial version landed)
- first-run certificate trust/bootstrap guidance is now available through packaged helper scripts
- WebView2 Runtime detection + helper scripts are now wired, but field validation is still pending
- end-to-end Wi-Fi Direct workflow automation
- reconnect/resume and broader stability work
- automated tests and CI still need broader field coverage
- optional future migration from the current Win32 shell to a Windows App SDK / WinUI-style product shell

See:

- [Development](docs/DEVELOPMENT.md)
- [Unfinished Features](docs/UNFINISHED_FEATURES.md)
- [Development Plan](docs/DEVELOPMENT_PLAN.md)
- [Signaling Protocol](docs/SIGNALING_PROTOCOL.md)
- [Windows Packaging](docs/WINDOWS_PACKAGING.md)
- [Windows Bootstrap Guide](docs/WINDOWS_BOOTSTRAP_GUIDE.md)

## Repository Layout

- `src/`
  - `core/` shared server, cert, protocol, util, and endpoint-selection rules
  - `platform/` Windows/POSIX provider, probe, and action implementations
- `www/`
  - `host.html`, `viewer.html`, and JS assets
- `src/desktop_host/`
  - desktop host app solution, project, native sources, and `webui/`
- `docs/desktop_host/`
  - desktop host build/status notes
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

- Server: `out/server/Debug/lan_screenshare_server.exe`
- Desktop app: `out/desktop_host/x64/Debug/LanScreenShareHostApp.exe`
- Build logs: `out/logs/build_*.log`

Server smoke test:

```powershell
.\scripts\windows\smoke_server.ps1 -Config Debug
```

Browser smoke test:

```powershell
.\scripts\windows\browser_smoke.ps1 -Config Debug
```

Desktop release validation:

```powershell
.\scripts\windows\validate_release.ps1 -Config Release
```

Linux build:

Requirements:

- `cmake`
- `ctest`
- `g++` or `clang++`
- `ninja` or `make`
- `curl`, `zip`, `unzip`, `tar`, `git` when using `--bootstrap-vcpkg`
- either:
  - a native Linux `vcpkg` checkout, or
  - system-installed CMake package configs for Boost/OpenSSL

Check Linux dependencies only:

```bash
./scripts/build_linux.sh --check-deps-only --skip-vcpkg-install
```

Build Linux server with native vcpkg bootstrap:

```bash
./scripts/build_linux.sh --target server --clean --bootstrap-vcpkg
```

Build everything and run tests on Linux:

```bash
./scripts/build_linux.sh --target all --run-tests --bootstrap-vcpkg
```

Linux staged outputs:

- Server: `out/linux/<Config>/server/lan_screenshare_server`
- Linux tray: `out/linux/<Config>/tray/lan_screenshare_linux_tray`

Packaging scripts:

- Windows zip package: `.\scripts\windows\package.ps1 -Config Release`
- Linux tarball package: `./scripts/package_linux.sh --config Release --vcpkg-root "$VCPKG_ROOT"`
- macOS app package: `./scripts/package_macos.sh --config Release --vcpkg-root "$VCPKG_ROOT"`

GitHub Actions packaging:

- `.github/workflows/package-platforms.yml` builds Windows, Linux, macOS Intel, and macOS Apple Silicon packages
- tag pushes matching `v*` upload the generated archives to the matching GitHub Release

## Runtime Notes

- The server depends on vcpkg runtime DLLs. The build script now copies them into the output directory.
- The build script now also validates the server output layout, runs server smoke by default for `-Target server`, runs the browser smoke target after server builds, and performs desktop release validation after desktop builds unless you explicitly skip those stages.
- Server certificate generation now uses the linked OpenSSL library through the platform provider layer instead of shelling out to `openssl.exe`.
- The desktop app can run without build-time WebView2 headers, but embedded host-page functionality is degraded in that environment.
- The embedded host page now only allows local self-signed certificate bypass for loopback / private-LAN URLs, and the Windows helper scripts can check WebView2 Runtime presence or import the generated certificate into the current-user root store.
- The desktop shell refreshes exported files in `out/share_bundle/` so the local share pages and diagnostics stay aligned with the current desktop snapshot.

## Suggested Next Work

The most valuable next steps are:

1. field-validate the packaged install/upgrade/uninstall flow on clean Windows machines
2. field-validate the WebView2 Runtime + certificate bootstrap helpers on real operator environments
3. harden reconnect/recovery and hotspot/manual-network UX
4. deepen browser automation and release validation on real Windows hardware


## Cross-platform runtime refactor status

- server CLI now resolves certificate and network concerns through provider interfaces under `src/platform/*`
- shared endpoint-selection rules now live in `src/core/network/endpoint_selection.*`
- Windows probe/action code now lives under `src/platform/windows/`
- Linux/macOS now have a POSIX probe baseline under `src/platform/posix/` for the CLI target


## Cross-platform refactor status

- Iter 5 introduced `src/core/runtime/runtime_controller.*` so desktop orchestration rules such as host/viewer URL generation, handoff-state evaluation, dashboard overall-state computation, and share-info composition are shared outside `MainWindow.cpp`.
- Iter 6 introduced `src/core/runtime/share_artifact_service.*` so share bundle generation, diagnostics export, and offline handoff-page emission no longer live inside `MainWindow.cpp`.


## Runtime refactor notes

- `src/core/runtime/desktop_runtime_snapshot.*` centralizes desktop runtime snapshot assembly from session + health signals.
- `src/platform/abstraction/platform_service_facade.h` routes desktop platform actions (hotspot control, system settings entry points, URL/path opening) through a reusable facade instead of keeping those branches inside `MainWindow.cpp`.

- `src/core/runtime/host_runtime_coordinator.*` + `src/platform/host_runtime_refresh_pipeline.*` now centralize probe refresh sequencing for the desktop host.
- `src/core/runtime/host_action_coordinator.*` now centralizes desktop user action sequencing for start/stop/open/export flows.

- Iter 11 complete: desktop host session/config editing and admin/backend sync now flow through `host_session_coordinator`.
- Iter 12 complete: admin snapshot shaping, dashboard cards, and settings read-only cards now flow through `admin_view_model_assembler`.
- Iter 13 complete: monitor page text, diagnostics page text, and HTML shell fallback messaging now flow through `diagnostics_view_model_assembler`.
- Iter 15 complete: setup-page form draft parsing, edit-dirty/pending-apply policy, and setup-page button labels now flow through `desktop_edit_session_presenter`.
- Iter 16 complete: tray/menu/balloon/status-tip shell chrome state now flows through `shell_chrome_presenter`, including tray command routing and shell-chrome view-model assembly.


## Recent refactor slices

- `desktop_shell_presenter`: centralizes native-page button routing and button-enable policy so `MainWindow.cpp` keeps less Win32-specific decision logic inline.
- `desktop_edit_session_presenter`: centralizes setup-page form-draft parsing, dirty/pending-apply detection, and button/summary policy for edits that have not yet been committed into the live host session.


- Native shell layout/state now also routes through a shared `desktop_layout_presenter`, which picks webview surface modes, grouped page visibility, and fallback resize geometry.


- Iter 18: shell bridge presenter / message adapter layer now owns admin-shell snapshot event JSON and host/admin bridge message parsing.


## Admin shell coordinator
The admin shell bridge now routes parsed admin commands through a shared admin shell coordinator so AdminBackend no longer owns command-to-action mapping directly.

The admin shell runtime publish path now flows through a shared admin shell runtime publisher so snapshot shaping, publish-trigger policy, and snapshot-event serialization no longer live directly inside `MainWindow.cpp` / `AdminBackend`.


## Iter 21 status

- Added `src/core/runtime/host_observability_coordinator.*` to centralize host-page status/log ingest, native log/timeline aggregation, poll result normalization, and diagnostics log filtering.


## Iter 22 status

- Completed: `src/core/runtime/host_runtime_scheduler.*` now centralizes runtime tick interval defaults plus the timer-tick policy for UI refresh and `/api/status` poll dispatch.
- `MainWindow.cpp` now routes `WM_TIMER` through `HandleRuntimeTick()` and the shared scheduler instead of hard-wiring `UpdateUiState()` and `KickPoll()` directly in the window procedure.
- Shared smoke coverage now includes `tests/shared/host_runtime_scheduler_tests.cpp`.

## Iter 23 status

- Completed: `src/core/runtime/host_shell_lifecycle_coordinator.*` now centralizes host shell startup, show/restore, minimize-to-tray, close, tray-exit, and destroy lifecycle plans.
- `MainWindow.cpp` now routes `Show()`, `RestoreFromTray()`, `MinimizeToTray()`, `WM_CLOSE`, tray Exit, and `OnDestroy()` through the shared lifecycle coordinator instead of duplicating those shell-flow decisions inline.
- Shared smoke coverage now includes `tests/shared/host_shell_lifecycle_coordinator_tests.cpp`.

- Desktop host Win32 shell creation is now split into `NativeControlFactory` and `DesktopHostPageBuilders`, so `MainWindow::OnCreate()` focuses on shell lifecycle bootstrap rather than raw control creation.

- Desktop host Iter 26 added `src/desktop_host/WebViewShellAdapter.*` so WebView2 initialization, embedded host/admin navigation, and restore-to-current-surface behavior are isolated from `MainWindow.cpp`.
- Desktop host Iter 27 added `src/desktop_host/ShellEffectExecutor.*` so lifecycle plan application, desktop/tray command execution, and tray icon/menu Win32 effects are isolated from `MainWindow.cpp`.

## Incremental update (2026-03-16, native shell debounce + notifications)

- Added `src/core/runtime/native_shell_alert_coordinator.*` to debounce health / viewer-count / unexpected-exit signals before Linux/macOS shells react.
- Added `src/core/runtime/native_shell_status_tracker.*` so future Linux tray and macOS menu-bar entries can consume stable shell chrome state plus queued notification events from one shared pipeline.
- Extended `shell_chrome_presenter` with richer detail/badge output and extended `PlatformServiceFacade` / `ISystemActions` with `ShowNotification(...)` so platform shells can raise native notifications without duplicating policy logic.


## Incremental update (2026-03-16, native Linux/macOS shell entry baseline)

- Added `src/host_shell/native_shell_live_poller.*`, `native_shell_runtime_loop.*`, and `native_shell_action_controller.*` so a lightweight Linux/macOS host shell can poll `/health` + `/api/status`, feed `native_shell_status_tracker`, and route notification events through `ShowNotification(...)`.
- Added `apps/linux_tray/main.cpp` as a real AppIndicator-style shell entry that dynamically loads GTK3/AppIndicator at runtime, refreshes status every 2 seconds, and exposes Dashboard / Viewer / Diagnostics actions from the tray menu.
- Added `apps/macos_menubar/main.mm` as a native Cocoa menu-bar entry that uses `NSStatusBar` + `NSTimer` to refresh stable shell state and route Dashboard / Viewer / Diagnostics actions through the shared controller layer.
- Added focused coverage with `tests/shared/native_shell_runtime_loop_tests.cpp` and `tests/shared/native_shell_action_controller_tests.cpp`.


## Incremental update (2026-03-16, native shell action closure)

- `src/host_shell/native_shell_action_controller.*` now owns a deeper Linux/macOS shell action closure: managed POSIX server start/stop, live-probe wait after start/stop, dashboard refresh validation against `/health` + `/api/status`, and diagnostics export reveal/open behavior.
- `NativeShellActionConfig` now carries managed server launch arguments plus diagnostics reveal policy, so native shells can request a more complete action lifecycle without re-implementing probe/reveal logic in entry apps.
- `tests/shared/native_shell_action_controller_tests.cpp` now covers refresh failure validation, managed start/stop with injected live snapshots, and diagnostics export reveal behavior.

## Incremental update (2026-03-16, native shell UX tightening)

- Linux tray now ties menu enable/disable state to debounced runtime health and stable viewer counts, and it surfaces action-result notifications through the shared `ShowNotification(...)` path.
- macOS now has a dedicated Cocoa system-actions provider (`src/platform/macos/system_actions_macos.mm`) so menu-bar notifications use native notification delivery instead of shelling out, and menu actions force an immediate runtime refresh after each command.


## Incremental update (2026-03-16, native shell action wiring)

- `apps/linux_tray/main.cpp` now routes tray menu actions through the deeper controller APIs: `RefreshDashboard(...)`, `StartServer(...)`, and `StopServer(...)`, and it accepts `--server-executable/--server-arg` so the native shell can actually launch a managed service process.
- `apps/macos_menubar/main.mm` now mirrors the same controller wiring for Dashboard refresh and managed start/stop, so both native shell entry points share the same live-probe wait / refresh validation semantics instead of keeping legacy shell-local action behavior.
