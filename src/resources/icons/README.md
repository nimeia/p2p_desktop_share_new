# Application Icons

Platform-specific icon resources integrated from flat_icon_pack_v2_states.

## Directory Structure

- **windows/** - Windows application and tray icons
  - `app_icon_v2.ico` - Main application icon
  - `tray_icon_light_v2.ico` - Light theme tray icon
  - `tray_icon_dark_v2.ico` - Dark theme tray icon
  - Tray state variants: `_alert_`, `_connected_`, `_sharing_`
  - `store/` - Windows Store / MSIX visual assets
  - `png/` - PNG variants regenerated from the checked-in master icon assets

- **macos/** - macOS application and menu bar icons
  - `AppIcon.icns` - Main application icon
  - `AppIcon.iconset/` - Icon set resources
  - `statusbar/` - Menu bar template icons
  - `statusbar_states/` - State variants

- **linux/** - Linux desktop and tray icons
  - `hicolor/` - Standard hicolor icon theme (16, 24, 32, 48, 64, 128, 256, 512)
  - `tray/` - Tray icons
  - `tray_states/` - State variants
  - `desktop/` - Desktop entry file and installation hints

## Usage

Refer to platform-specific implementations in `src/platform/windows/`, `src/platform/macos/`, and `src/platform/posix/` for integration details.

Rebuild the checked-in desktop app icons with:

`powershell -ExecutionPolicy Bypass -File scripts/regenerate_icons.ps1`
