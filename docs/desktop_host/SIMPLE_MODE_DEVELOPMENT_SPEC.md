# Simple Mode Development Spec

## Scope of this iteration
This iteration lands three concrete pieces:

1. **Dashboard handoff state** inside the HTML Admin Shell.
2. **Wizard linkage state** inside the native host.
3. **Windows tray integration** for common operator actions.

## Native host changes
### New state tracked in `MainWindow`
- `m_shareWizardOpened`
- `m_shareWizardHandoffStarted`
- `m_wizardHandoffState`
- `m_wizardHandoffLabel`
- `m_wizardHandoffDetail`
- tray state fields (`m_trayInstalled`, `m_exitRequested`, `m_trayBalloonShown`, `m_trayIcon`)

### New native helpers
- `EnsureTrayIcon`
- `RemoveTrayIcon`
- `UpdateTrayIcon`
- `MinimizeToTray`
- `RestoreFromTray`
- `ShowTrayMenu`
- `HandleTrayMessage`
- `UpdateWizardHandoffState`

### Wizard handoff state rules
`UpdateWizardHandoffState()` computes:
- `idle` when wizard never opened,
- `ready` when wizard is open and no blockers are present,
- `delivered` when handoff started or a viewer connected,
- `needs-fix` when wizard is open but service / cert / reachability / P0 checks block delivery.

## Admin bridge changes
### Snapshot additions
The HTML shell now receives:
- `shareWizardOpened`
- `shareWizardHandoffStarted`
- `wizardState`
- `wizardLabel`
- `wizardDetail`

### New bridge command
- `show-qr`

## HTML shell changes
### Dashboard additions
A new **Share Wizard Handoff** card is rendered from bridge snapshot state.

### Dynamic fix button
The Dashboard computes the most suitable next action:
- `Start Sharing`
- `Refresh Network`
- `Open Diagnostics`
- `Re-run Checks`
- `Open Monitor`
- `Open Sharing`

## Tray integration
### Native messages
- uses `WM_APP_TRAY`
- uses `Shell_NotifyIconW`
- minimizes / closes to tray
- restores on tray click

### Menu commands
- open dashboard
- start sharing
- stop sharing
- copy viewer url
- show qr
- open share wizard
- exit

## Validation focus
### Static validation done in sandbox
- JS syntax check for `src/desktop_host/webui/app.js`
- manual inspection of bridge command coverage
- manual inspection of tray command routing

### Windows validation still required
- minimize-to-tray behavior
- close-to-tray behavior
- tray double-click restore
- tray action routing
- Dashboard handoff state transitions during real sharing
