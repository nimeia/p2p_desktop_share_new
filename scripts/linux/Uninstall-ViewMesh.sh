#!/usr/bin/env bash

set -euo pipefail

REMOVE_DATA=0
PREFIX=""

usage() {
  cat <<'EOF'
Usage: Uninstall-ViewMesh.sh [options]

Options:
  --prefix <path>   Override install root instead of using this script location
  --remove-data     Also remove XDG state data under ~/.local/state/viewmesh
  -h, --help        Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      PREFIX="$2"
      shift 2
      ;;
    --remove-data)
      REMOVE_DATA=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf '[uninstall_linux][error] Unknown argument: %s\n' "$1" >&2
      exit 1
      ;;
  esac
done

INSTALL_ROOT="${PREFIX:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
rm -f "$HOME/.local/share/applications/viewmesh.desktop"
rm -f "/usr/local/share/applications/viewmesh.desktop"
rm -rf "$INSTALL_ROOT"

if [[ "$REMOVE_DATA" -eq 1 ]]; then
  rm -rf "${XDG_STATE_HOME:-$HOME/.local/state}/viewmesh"
  rm -rf "${XDG_STATE_HOME:-$HOME/.local/state}/lan_screenshare"
fi

printf 'Removed %s\n' "$INSTALL_ROOT"
