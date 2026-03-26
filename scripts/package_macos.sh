#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/package_common.sh"
ROOT_DIR="$(package_repo_root)"
SCOPE="package_macos"

CONFIG="Release"
VERSION=""
OUTPUT_ROOT="$ROOT_DIR/out/package/macos"
BUILD_DIR=""
VCPKG_ROOT_INPUT="${VCPKG_ROOT:-}"
SKIP_BUILD=0
BOOTSTRAP_VCPKG=0

usage() {
  cat <<'EOF'
Usage: scripts/package_macos.sh [options]

Options:
  --config Debug|Release   Build configuration. Default: Release
  --version <value>        Override package version text
  --output-root <path>     Package output root. Default: out/package/macos
  --build-dir <path>       CMake build directory override
  --vcpkg-root <path>      vcpkg checkout root
  --skip-build             Reuse an existing build directory
  --bootstrap-vcpkg        Run bootstrap-vcpkg.sh when needed
  -h, --help               Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config)
      require_option_value "$SCOPE" "$1" "${2-}"
      CONFIG="$2"
      shift 2
      ;;
    --version)
      require_option_value "$SCOPE" "$1" "${2-}"
      VERSION="$2"
      shift 2
      ;;
    --output-root)
      require_option_value "$SCOPE" "$1" "${2-}"
      OUTPUT_ROOT="$2"
      shift 2
      ;;
    --build-dir)
      require_option_value "$SCOPE" "$1" "${2-}"
      BUILD_DIR="$2"
      shift 2
      ;;
    --vcpkg-root)
      require_option_value "$SCOPE" "$1" "${2-}"
      VCPKG_ROOT_INPUT="$2"
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
      package_fail "$SCOPE" "Unknown argument: $1"
      ;;
  esac
done

validate_package_config "$SCOPE" "$CONFIG"

ARCH="$(map_machine_arch "$(uname -m)")"
TRIPLET="$([[ "$ARCH" == "arm64" ]] && printf 'arm64-osx' || printf 'x64-osx')"
if [[ -z "$BUILD_DIR" ]]; then
  BUILD_DIR="$ROOT_DIR/out/build/macos-ninja-${TRIPLET}-${CONFIG}"
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  require_vcpkg_root "$SCOPE" "$VCPKG_ROOT_INPUT"
  VCPKG_EXE="$(ensure_vcpkg_executable "$SCOPE" "$VCPKG_ROOT_INPUT" "$BOOTSTRAP_VCPKG")"

  mkdir -p "$BUILD_DIR"
  (
    cd "$ROOT_DIR"
    "$VCPKG_EXE" install --triplet "$TRIPLET"
  )
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT_INPUT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="$TRIPLET"
  cmake --build "$BUILD_DIR" --target lan_screenshare_server lan_screenshare_macos_menubar
fi

APP_BUNDLE="$BUILD_DIR/ViewMesh.app"
SERVER_BIN="$BUILD_DIR/ViewMeshServer"
require_path "$APP_BUNDLE" "macOS app bundle"
require_path "$SERVER_BIN" "macOS server binary"
require_path "$ROOT_DIR/www" "web root"
require_path "$ROOT_DIR/src/desktop_host/webui" "admin webui"

VERSION_RESOLVED="$(resolve_package_version "$ROOT_DIR" "$VERSION")"
PACKAGE_NAME="ViewMesh_${VERSION_RESOLVED}_macos-${ARCH}"
STAGE_DIR="$OUTPUT_ROOT/$PACKAGE_NAME"
APP_NAME="ViewMesh.app"
PACKAGED_APP="$STAGE_DIR/$APP_NAME"
RUNTIME_DIR="$PACKAGED_APP/Contents/Resources/runtime"
DMG_ROOT="$OUTPUT_ROOT/${PACKAGE_NAME}_dmg_root"
DMG_PATH="$OUTPUT_ROOT/${PACKAGE_NAME}.dmg"
ZIP_PATH="$OUTPUT_ROOT/${PACKAGE_NAME}.zip"

rm -rf "$STAGE_DIR" "$DMG_ROOT"
rm -f "$DMG_PATH" "$ZIP_PATH"
mkdir -p "$STAGE_DIR" "$DMG_ROOT"

cp -R "$APP_BUNDLE" "$PACKAGED_APP"
mkdir -p "$RUNTIME_DIR"
cp "$SERVER_BIN" "$RUNTIME_DIR/ViewMeshServer"
cp -R "$ROOT_DIR/www" "$RUNTIME_DIR/www"
cp -R "$ROOT_DIR/src/desktop_host/webui" "$RUNTIME_DIR/webui"
chmod +x "$RUNTIME_DIR/ViewMeshServer"

GENERATED_AT="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
cat > "$STAGE_DIR/package_manifest.json" <<EOF
{
  "product": "ViewMesh",
  "version": "$(write_json_string "$VERSION_RESOLVED")",
  "platform": "macos",
  "arch": "$(write_json_string "$ARCH")",
  "config": "$(write_json_string "$CONFIG")",
  "generated_at": "$GENERATED_AT",
  "package_type": "dmg",
  "app_bundle": "$(write_json_string "$APP_NAME")"
}
EOF
cp "$STAGE_DIR/package_manifest.json" "$RUNTIME_DIR/package_manifest.json"

cp -R "$PACKAGED_APP" "$DMG_ROOT/$APP_NAME"
ln -s /Applications "$DMG_ROOT/Applications"

mkdir -p "$OUTPUT_ROOT"
ditto -c -k --sequesterRsrc --keepParent "$PACKAGED_APP" "$ZIP_PATH"
hdiutil create -volname "ViewMesh" -srcfolder "$DMG_ROOT" -format UDZO "$DMG_PATH"

print_package_result "StageDir" "$STAGE_DIR"
print_package_result "DMG" "$DMG_PATH"
print_package_result "Zip" "$ZIP_PATH"
