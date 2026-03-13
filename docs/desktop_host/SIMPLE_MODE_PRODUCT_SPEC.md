# Simple Mode Product Spec

## Goal
Provide a shorter operator path for the desktop host by treating the HTML **Dashboard** as the lightweight default entry. The operator should be able to:

1. start sharing,
2. hand off the Viewer URL or QR,
3. see whether the handoff is already delivered or still blocked,
4. jump to the right fix path.

## Current implementation boundary
This iteration does **not** add a separate native window. It reuses the existing HTML Admin Shell and upgrades the Dashboard to act as the lightweight mode.

## Key UX rules
- The Dashboard is the first screen.
- Share Wizard is the guided handoff flow.
- Returning from Share Wizard should surface a clear state:
  - **Not started**
  - **Ready For Handoff**
  - **Delivered**
  - **Needs Fix**
- The tray menu must expose the shortest operator actions.

## Dashboard handoff card
The Dashboard now includes a dedicated **Share Wizard Handoff** card with:
- current handoff state badge,
- detail text,
- summary rows,
- actions for:
  - Open Share Wizard,
  - Show QR / Share Card,
  - Copy Viewer URL,
  - a context-sensitive fix action.

## Handoff state model
### Not started
The wizard has not been opened yet.

### Ready For Handoff
Wizard has been opened and there are no major blockers.

### Delivered
The handoff has started or a viewer has already connected.

### Needs Fix
Wizard was opened but sharing health still has a blocking issue.

## Tray behavior
The tray is used as the shortest operator surface.

### Supported actions
- Open Dashboard
- Start Sharing
- Stop Sharing
- Copy Viewer URL
- Show QR / Share Card
- Open Share Wizard
- Exit

### Window behavior
- Minimize hides to tray.
- Close hides to tray unless the user explicitly exits from the tray menu.
- Double-click tray restores the window.

## Share Wizard linkage
Share Wizard remains the offline handoff page, but now explicitly tells the operator to return to the desktop dashboard or tray after handoff. The desktop host then computes the current handoff state from:
- whether Share Wizard was opened,
- whether handoff actions started,
- whether a viewer connected,
- current local health / certificate / reachability blockers.
