#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
STATE_ROOT="${XDG_STATE_HOME:-$HOME/.local/state}/lan_screenshare"
DIAGNOSTICS_DIR="$STATE_ROOT/out/diagnostics"

mkdir -p "$DIAGNOSTICS_DIR"

exec "$ROOT_DIR/bin/lan_screenshare_linux_tray" \
  --server-executable "$ROOT_DIR/runtime/lan_screenshare_server" \
  --diagnostics-dir "$DIAGNOSTICS_DIR" \
  "$@"
