#!/usr/bin/env bash

set -euo pipefail

SCOPE="user"
PREFIX=""

usage() {
  cat <<'EOF'
Usage: Install-LanScreenShare.sh [options]

Options:
  --scope user|system   Install scope. Default: user
  --prefix <path>       Override install root
  -h, --help            Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --scope)
      SCOPE="$2"
      shift 2
      ;;
    --prefix)
      PREFIX="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf '[install_linux][error] Unknown argument: %s\n' "$1" >&2
      exit 1
      ;;
  esac
done

case "$SCOPE" in
  user|system) ;;
  *)
    printf '[install_linux][error] Unsupported scope: %s\n' "$SCOPE" >&2
    exit 1
    ;;
esac

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$SCOPE" == "system" ]]; then
  INSTALL_ROOT="${PREFIX:-/opt/LanScreenShareHost}"
  DESKTOP_DIR="/usr/local/share/applications"
else
  INSTALL_ROOT="${PREFIX:-$HOME/.local/share/LanScreenShareHost}"
  DESKTOP_DIR="$HOME/.local/share/applications"
fi

rm -rf "$INSTALL_ROOT"
mkdir -p "$INSTALL_ROOT" "$DESKTOP_DIR"

cp -R "$PACKAGE_DIR/bin" "$INSTALL_ROOT/"
cp -R "$PACKAGE_DIR/runtime" "$INSTALL_ROOT/"
cp -R "$PACKAGE_DIR/icons" "$INSTALL_ROOT/"
cp "$PACKAGE_DIR/package_manifest.json" "$INSTALL_ROOT/"
cp "$PACKAGE_DIR/Install-LanScreenShare.sh" "$INSTALL_ROOT/"
cp "$PACKAGE_DIR/Uninstall-LanScreenShare.sh" "$INSTALL_ROOT/"

chmod +x "$INSTALL_ROOT/bin/lan_screenshare_linux_tray"
chmod +x "$INSTALL_ROOT/bin/launch_lan_screenshare_linux_tray.sh"
chmod +x "$INSTALL_ROOT/runtime/lan_screenshare_server"
chmod +x "$INSTALL_ROOT/Install-LanScreenShare.sh"
chmod +x "$INSTALL_ROOT/Uninstall-LanScreenShare.sh"

cat > "$DESKTOP_DIR/lanscreenshare.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=LanScreenShare
Comment=Desktop sharing host
Exec=$INSTALL_ROOT/bin/launch_lan_screenshare_linux_tray.sh
Icon=$INSTALL_ROOT/icons/linux/hicolor/256x256/apps/lanscreenshare.png
Terminal=false
Categories=Network;Utility;
StartupNotify=true
EOF

printf 'Installed to %s\n' "$INSTALL_ROOT"
printf 'Desktop entry: %s/lanscreenshare.desktop\n' "$DESKTOP_DIR"
