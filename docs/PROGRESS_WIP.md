# WIP Package Progress (v3)

This package is intentionally **not finished**. It is provided to prevent the chat from becoming too long.

## What is included
- Desktop host app (Win32 shell, unpackaged):
  - `src/desktop_host/`
  - Basic control panel: start/stop server process, open Host/Viewer URL, open output dirs/logs.
- Server-side incremental improvements (C++ core):
  - ServiceHost now initializes Listener + HttpRouter (no longer TODO).
  - `/api/status` endpoint (returns rooms/viewers from hub).
  - WebSocket: host can send `session.end` -> viewers receive `session.ended`.
  - Viewer limit enforced (`ServiceConfig.maxViewers` -> `ROOM_FULL`).
  - WebSocket Send uses a serialized queue to avoid concurrent write issues.
  - Listener supports Stop().
  - Signal/HTTP JSON responses built via `src/core/protocol/messages.*`.

- Web front-end (host/viewer pages):
  - Viewer now supports **multiple offers** (renegotiation) using a single RTCPeerConnection.
  - Host now renegotiates after `getDisplayMedia()` / `addTrack` so the flow works when **viewer joins first**.
  - Host stop-share triggers `session.end` and viewer handles `session.ended` (explicit end state + cleanup).

## What is NOT finished (next pack)
- Desktop host Step2 real Wi-Fi Direct guidance + Host IPv4 detection.
- Desktop host Step3 spawn `ViewMeshServer.exe` and poll `/api/status`.
- Desktop host Step4 embed WebView2 host page and allow self-signed cert.
- Desktop host Step5 generate QR and copy/open buttons.
- The original `CertManager` has been split for the CLI path, but `NetworkManager` still needs the same treatment.

## Quick notes
- Generate self-signed cert via:
  - `scripts/gen_self_signed_cert.ps1 -OutDir .\cert -SanIp <host-ip>` for legacy/manual flows
  - The current server CLI path now generates certificates in-process through the linked OpenSSL library, so it no longer depends on locating `openssl.exe` first.

## Build fixes (2026-01-28)
- Fixed missing Boost.Beast/Asio includes:
  - `boost/asio/strand.hpp` for `boost::asio::make_strand`
  - `boost/beast/core/tcp_stream.hpp` for `boost::beast::tcp_stream`
  - `boost/beast/core/buffers_to_string.hpp` for `boost::beast::buffers_to_string`
- Switched JSON parse error reporting to `boost::system::error_code` (Boost.JSON expects this).
- Marked `WsHub::mu_` as `mutable` so `GetStats() const` can lock safely.
- Adjusted strand usage in `WsSession`: use `boost::asio::any_io_executor` returned by `boost::asio::make_strand(...)`
  to avoid `strand<any_io_executor>` template instantiation issues on MSVC + Boost 1.90.
- Defined `_WIN32_WINNT=0x0A00` for Windows 10+ APIs (silences Asio's default Windows 7 assumption warning).
  (Removed outdated note: this package no longer contains a .NET desktop app scaffold; the current desktop target is a native Win32 shell.)

## Incremental update (2026-03-08)
- `NetworkManager` expanded beyond plain adapter probing:
  - Added `QueryCapabilities()` for Wi-Fi adapter / hotspot / Wi-Fi Direct API capability probing
  - Added `MakeSuggestedHotspotConfig()` for default SSID/password generation
  - Added best-effort `StartHotspot()` / `StopHotspot()` / `QueryHotspotState()` using Windows hosted-network commands
- Win32 host shell improvements:
  - Added hotspot controls (SSID / password / start / stop)
  - Added Wi-Fi Direct pairing entry that opens the Windows pairing UI
  - Capability summary now shows Wi-Fi adapter presence, hotspot support, and Wi-Fi Direct API availability
  - Share info panel now includes hotspot state / SSID / password / Wi-Fi Direct status
- Share card is now fully local/offline:
  - Removed dependency on online QR image services
  - Generates a local `share_card.html` with Viewer URL, hotspot credentials, and offline connection instructions
  - Copy/open actions are all handled locally in the generated HTML
- Web front-end host page improvements retained:
  - Native status sync hooks for `idle / ready / sharing / closed / error`
  - Viewer count updates are pushed back to the desktop shell

## Incremental update (2026-03-08, continued)
- Offline share card now includes a **real locally generated QR**:
  - Added `www/assets/share_card_qr.bundle.js`
  - The bundle is adapted from the local QRCode-for-JavaScript implementation shipped inside `qrcode-terminal` vendor sources
  - Share card renders SVG QR locally in-browser and can download `viewer_qr.svg`
- Desktop shell quality-of-life improvements:
  - Added `Hotspot Settings` button that opens Windows Mobile Hotspot settings as the fallback path when hosted-network commands are unavailable
  - Share card generation now warns in the host log if the local QR asset file is missing from `www/assets`
  - Capability text now explicitly points users to Windows Mobile Hotspot settings when direct hosted-network control is not detected
- Third-party notice:
  - Added `third_party/licenses/qrcode-js-MIT.txt` to document the bundled QR encoder attribution

## Still not finished
- The share card now includes a locally generated SVG QR, but it is still rendered in-browser from local JS rather than produced by the native desktop host itself.
- Hotspot control is best-effort via hosted-network commands; true productized Mobile Hotspot / Wi-Fi Direct session orchestration is still pending.
- The desktop shell is still Win32 + WebView2 rather than a Windows App SDK / WinUI-style application.


## Incremental update (2026-03-08, share bundle / wizard)
- Desktop host now produces a fuller **local share bundle** rather than only a single share card:
  - `share_card.html`
  - `share_wizard.html`
  - `share_bundle.json`
  - `viewer_url.txt`
  - `hotspot_credentials.txt`
  - `share_readme.txt`
- Added a productized **Share Wizard** entry in the desktop shell:
  - richer offline handoff page
  - local QR rendering
  - server / host / viewer checklist
  - LAN / hotspot / Wi-Fi Direct guidance in one page
- Added a machine-readable share manifest (`share_bundle.json`) so later automation can consume current room / token / URLs / hotspot state.
- Wi-Fi Direct is still not implemented as a native media transport path, but the session handoff is now more explicit:
  - generated local session alias (`ViewMesh-<room>`)
  - pairing entry guidance
  - fallback instructions included in the bundle


## Incremental update (2026-03-09, source-pack linkage + exported bundle hardening)
- Filled in the previously missing `MainWindow` source implementations that were declared/called by the UI shell but absent from the source pack:
  - hotspot start/stop/state refresh
  - host/viewer open + copy helpers
  - share card / share wizard / export bundle / open output folder
  - bundle-writing, log appending, WebView navigation, and basic UI-state refresh
- Hardened exported share bundle layout:
  - writes to `out/share_bundle/`
  - copies local QR asset into `out/share_bundle/www/assets/`
  - keeps generated `share_card.html` / `share_wizard.html` self-contained relative to the bundle directory
- Added extra exported support files:
  - `share_status.js`
  - `share_diagnostics.txt`
- `RefreshShareInfo()` now also refreshes the exported share artifacts so the bundle contents stay aligned with the latest desktop-side state snapshot.

## Incremental update (2026-03-09, live share pages)
- The exported offline share pages are no longer just static snapshots:
  - `share_card.html` now embeds the current bundle JSON snapshot and polls `share_status.js`
  - `share_wizard.html` does the same and refreshes room/viewer counts, host state, hotspot state, and the QR target URL in place
- `share_status.js` now publishes a browser event (`lan-share-status`) in addition to updating `window.__LAN_SHARE_STATUS__`, so already-open local pages can react as soon as the refreshed script loads.
- Share bundle metadata was expanded with a few extra fields useful to the live pages:
  - `statusVersion`
  - `refreshMs`
  - `serverRunning`
  - `bundle.statusScript / readme / diagnostics`
- `share_diagnostics.txt` and `share_readme.txt` now explicitly document the live-page behavior and how to recover if a local page looks stale.
- Desktop share info panel now also tells the user that the exported pages auto-refresh from `share_status.js` and lists the additional bundle files.


## Incremental update (2026-03-09, share wizard diagnostics)
- Share Wizard is now closer to a **diagnostic wizard** rather than only a handoff page:
  - added certificate status section (cert dir / cert file / key file / trust hint)
  - added dynamic self-check cards for server, host sharing, host network, viewer URL, certificate files, and live bundle refresh
  - added targeted “common failure scenarios” that change with the current exported state instead of showing only static help text
- `share_bundle.json` now exports extra diagnostic metadata:
  - certificate file paths + presence flags
  - precomputed check booleans for server / host / network / viewer / certificate / live bundle status
- `share_diagnostics.txt` now records:
  - certificate file presence
  - self-check summary
  - targeted troubleshooting hints for the current state snapshot
- Desktop-side share info also now surfaces whether certificate files look ready, so missing cert/key material is easier to spot before handing the bundle to another user.

## Incremental update (2026-03-09, desktop self-check + shared diagnostics model)
- Added desktop-side self-check actions to the Win32 host shell:
  - `Run Self-Check`
  - `Open Diag`
- Exported bundle is expanded again with desktop-operator focused outputs:
  - `desktop_self_check.html`
  - `desktop_self_check.txt`
- Introduced a shared self-check model in the desktop source:
  - the exported `share_bundle.json` now carries a `selfCheck` section with passed/total counts, per-check items, and targeted issue hints
  - desktop-side report generation reuses the same rule set instead of inventing a second diagnostics path
- Share info panel now surfaces a compact self-check summary and lists the expanded bundle contents.
- `share_diagnostics.txt`, `desktop_self_check.html`, and `desktop_self_check.txt` now stay aligned with the same exported state snapshot.

## Incremental update (2026-03-09, wizard now consumes shared self-check + re-run workflow)
- The Win32 host shell now has a more explicit diagnostics workflow:
  - renamed/opened the bundle location more clearly as `Open Bundle Folder`
  - added `Re-run Checks`, which regenerates the exported bundle, self-check pages, and diagnostics files without forcing a fresh share/export action
- Share Wizard was tightened to consume the exported `selfCheck.items` / `selfCheck.issues` model first, instead of re-deriving a separate set of per-card rules:
  - the diagnostic grid is now rendered from the shared exported self-check items
  - this reduces drift between `share_wizard.html`, `desktop_self_check.html`, and `share_diagnostics.txt`
- Share Wizard also gained a small operator workflow layer:
  - added links to `desktop_self_check.html` and `desktop_self_check.txt`
  - added a “Suggested next actions” section derived from the currently failing self-check items
- Desktop-side share info text now explicitly mentions the `Run Self-Check` / `Re-run Checks` flow so operators know how to refresh diagnostics after state changes.

## Incremental update (2026-03-09, severity-based diagnostics + category filters)
- The shared self-check model now carries richer metadata per item and per issue:
  - `severity` (`P0` / `P1` / `P2`)
  - `category` (`certificate` / `network` / `sharing`)
  - aggregate `severityCounts` and `categoryCounts` in `share_bundle.json`
- `desktop_self_check.html` was upgraded from a plain checklist to a filterable diagnostics console:
  - added summary pills for `P0` / `P1` / `P2`
  - added category summary pills for certificate / network / sharing
  - added category and severity filter bars so operators can narrow the report to one problem class while on a support call
- `share_wizard.html` now surfaces the same severity/category model in its diagnostic area:
  - summary pills show current severity and category counts
  - diagnostic cards and issue hints display severity/category badges
  - wizard-side filters now narrow the diagnostic grid and issue list using the same exported self-check snapshot
- Plain-text reports were also aligned to the richer model:
  - `desktop_self_check.txt`
  - `share_diagnostics.txt`
  now include severity/category annotations and aggregate counts instead of only pass/fail text
- Desktop-side share info summary now includes compact category counts so the operator can spot whether the current risk is mainly certificate, network, or sharing related before opening the detailed report.

## Incremental update (2026-03-09, main-window diagnostic summary + operator first actions)
- The Win32 host shell left pane now surfaces a more operator-friendly diagnostics snapshot directly in the main window:
  - added a `Diagnostic Summary` box with overall pass counts, P0/P1/P2 totals, category totals, top issue, and a simple go/no-go decision line
  - added an `Operator First Actions` box that promotes the top targeted remediation steps, prioritizing P0 blockers before P1/P2 items
- The shared self-check failure hints now carry an explicit action string, so the desktop host can show a more concrete “do this next” instruction instead of repeating only the problem description.
- Plain-text outputs were extended to mirror the same operator guidance:
  - `desktop_self_check.txt` now includes an `Operator first actions` section
  - `share_diagnostics.txt` now includes the same `Operator first actions` section
- `share_bundle.json` issue entries now also carry an `action` field, so future UI/report views can consume the same next-step guidance without inventing a second rule layer.
- Desktop-side `Share Info` summary now adds an explicit operator focus line (`clear P0 first` / `clear P1 next` / `ready to share`) so the main window itself becomes a triage surface, not just a launch pad.

## Incremental update (2026-03-09, unified action consumption in wizard + desktop self-check)
- Tightened the “what should I do next?” path across the exported diagnostics views:
  - `share_wizard.html` now derives its `Suggested next actions` list from exported `selfCheck.issues[].action` entries first, instead of regenerating a separate hard-coded action list
  - action rows now also surface severity/category badges and preserve the underlying problem detail as a short “Why” note when the action text differs from the issue description
- `desktop_self_check.html` now gained its own `Suggested next actions` section:
  - it consumes the same exported `selfCheck.issues[].action` data used by Share Wizard
  - category/severity filters now affect both the open-issue list and the action list, so operators can isolate only `P0 network`, `certificate`, etc.
- Open-issue rendering in the desktop report was also improved:
  - individual issue rows now show the associated `Action:` line when the bundle contains an explicit remediation string
- This reduces another remaining drift point between:
  - main-window `Operator First Actions`
  - `share_wizard.html`
  - `desktop_self_check.html`
  - `share_diagnostics.txt`
  because all of them now prefer the same exported issue/action guidance instead of recomputing separate next-step text.


## Incremental update (2026-03-13, dashboard handoff + tray hardening)
- Added a first productized bridge between the exported Share Wizard workflow and the desktop Dashboard:
  - snapshot payload now includes `shareWizardOpened`, `handoffStarted`, `handoffDelivered`, and a computed `handoffState / handoffLabel / handoffDetail` summary
  - Dashboard now renders a dedicated `Share Wizard Handoff` card so the operator can tell whether the handoff has not started yet, is ready for handoff, needs fix, or is already delivered
- Added dashboard-side `Quick Fix` actions with native command routing instead of only passive hints:
  - `quick-fix-network` refreshes adapter detection and opens the Network page
  - `quick-fix-certificate` refreshes diagnostics and opens the report path
  - `quick-fix-sharing` restarts the share/start flow
  - `quick-fix-handoff` refreshes the current QR / Viewer URL handoff material
  - `quick-fix-hotspot` routes to hotspot fallback handling
- Added a real Win32 system tray baseline for the desktop host:
  - tray icon add/remove lifecycle
  - minimize-to-tray / close-to-tray behavior
  - restore-on-click behavior
  - tray tooltip status refresh
  - tray menu actions for Dashboard / Start Sharing / Stop Sharing / Copy Viewer URL / Show QR / Open Share Wizard / Exit
- This round is still a code-level hardening pass rather than a true local Windows runtime verification pass; final behavior still needs validation on a Windows machine with WebView2 and the packaged server binary present.
