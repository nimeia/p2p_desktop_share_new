#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/package_common.sh"

CONFIG="Release"
VERSION=""
OUTPUT_ROOT="$ROOT_DIR/out/package/linux"
VCPKG_ROOT_INPUT="${VCPKG_ROOT:-auto}"
BUILD_ROOT="auto"
SKIP_BUILD=0
BOOTSTRAP_VCPKG=0

usage() {
  cat <<'EOF'
Usage: scripts/package_linux.sh [options]

Options:
  --config Debug|Release         Build configuration. Default: Release
  --version <value>              Override package version text
  --output-root <path>           Package output root. Default: out/package/linux
  --vcpkg-root <path>|auto|none  vcpkg root forwarded to build_linux.sh
  --build-root <path>|auto       Build root forwarded to build_linux.sh
  --skip-build                   Reuse existing out/linux/<Config> outputs
  --bootstrap-vcpkg              Forward bootstrap flag to build_linux.sh
  -h, --help                     Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config)
      CONFIG="$2"
      shift 2
      ;;
    --version)
      VERSION="$2"
      shift 2
      ;;
    --output-root)
      OUTPUT_ROOT="$2"
      shift 2
      ;;
    --vcpkg-root)
      VCPKG_ROOT_INPUT="$2"
      shift 2
      ;;
    --build-root)
      BUILD_ROOT="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --bootstrap-vcpkg)
      BOOTSTRAP_VCPKG=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf '[package_linux][error] Unknown argument: %s\n' "$1" >&2
      exit 1
      ;;
  esac
done

case "$CONFIG" in
  Debug|Release) ;;
  *)
    printf '[package_linux][error] Unsupported config: %s\n' "$CONFIG" >&2
    exit 1
    ;;
esac

VERSION_RESOLVED="$(resolve_package_version "$ROOT_DIR" "$VERSION")"
ARCH="$(map_machine_arch "$(uname -m)")"
PACKAGE_NAME="LanScreenShareHost_${VERSION_RESOLVED}_linux-${ARCH}"
STAGE_DIR="$OUTPUT_ROOT/$PACKAGE_NAME"
ARCHIVE_PATH="$OUTPUT_ROOT/${PACKAGE_NAME}.tar.gz"
SERVER_DIR="$ROOT_DIR/out/linux/$CONFIG/server"
TRAY_DIR="$ROOT_DIR/out/linux/$CONFIG/tray"
ICON_DIR="$ROOT_DIR/src/resources/icons/linux"

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  build_args=(--config "$CONFIG" --target all)
  if [[ "$VCPKG_ROOT_INPUT" != "auto" || -n "${VCPKG_ROOT:-}" ]]; then
    build_args+=(--vcpkg-root "$VCPKG_ROOT_INPUT")
  fi
  if [[ "$BUILD_ROOT" != "auto" ]]; then
    build_args+=(--build-root "$BUILD_ROOT")
  fi
  if [[ "$BOOTSTRAP_VCPKG" -eq 1 ]]; then
    build_args+=(--bootstrap-vcpkg)
  fi
  bash "$ROOT_DIR/scripts/build_linux.sh" "${build_args[@]}"
fi

require_path "$SERVER_DIR/lan_screenshare_server" "Linux server binary"
require_path "$SERVER_DIR/www" "Linux server web root"
require_path "$SERVER_DIR/webui" "Linux admin webui"
require_path "$TRAY_DIR/lan_screenshare_linux_tray" "Linux tray binary"
require_path "$ICON_DIR" "Linux icon assets"
require_path "$ROOT_DIR/scripts/linux/Install-LanScreenShare.sh" "Linux install script"
require_path "$ROOT_DIR/scripts/linux/Uninstall-LanScreenShare.sh" "Linux uninstall script"
require_path "$ROOT_DIR/scripts/linux/launch_lan_screenshare_linux_tray.sh" "Linux launcher script"

rm -rf "$STAGE_DIR"
rm -f "$ARCHIVE_PATH"
mkdir -p "$STAGE_DIR/bin" "$STAGE_DIR/runtime" "$STAGE_DIR/icons"

cp "$TRAY_DIR/lan_screenshare_linux_tray" "$STAGE_DIR/bin/"
cp "$ROOT_DIR/scripts/linux/launch_lan_screenshare_linux_tray.sh" "$STAGE_DIR/bin/"
cp -R "$SERVER_DIR/." "$STAGE_DIR/runtime/"
cp -R "$ICON_DIR" "$STAGE_DIR/icons/linux"
cp "$ROOT_DIR/scripts/linux/Install-LanScreenShare.sh" "$STAGE_DIR/"
cp "$ROOT_DIR/scripts/linux/Uninstall-LanScreenShare.sh" "$STAGE_DIR/"

chmod +x "$STAGE_DIR/bin/lan_screenshare_linux_tray"
chmod +x "$STAGE_DIR/bin/launch_lan_screenshare_linux_tray.sh"
chmod +x "$STAGE_DIR/Install-LanScreenShare.sh"
chmod +x "$STAGE_DIR/Uninstall-LanScreenShare.sh"
chmod +x "$STAGE_DIR/runtime/lan_screenshare_server"

GENERATED_AT="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
cat > "$STAGE_DIR/package_manifest.json" <<EOF
{
  "product": "LanScreenShareHost",
  "version": "$(write_json_string "$VERSION_RESOLVED")",
  "platform": "linux",
  "arch": "$(write_json_string "$ARCH")",
  "config": "$(write_json_string "$CONFIG")",
  "generated_at": "$GENERATED_AT",
  "package_type": "tar.gz",
  "entrypoint": "bin/launch_lan_screenshare_linux_tray.sh"
}
EOF

mkdir -p "$OUTPUT_ROOT"
tar -czf "$ARCHIVE_PATH" -C "$OUTPUT_ROOT" "$PACKAGE_NAME"

printf 'StageDir: %s\n' "$STAGE_DIR"
printf 'Archive:  %s\n' "$ARCHIVE_PATH"
