#!/usr/bin/env bash

set -euo pipefail

package_repo_root() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  cd "$script_dir/.." && pwd
}

resolve_package_version() {
  local repo_root="$1"
  local requested="${2:-}"
  if [[ -n "$requested" ]]; then
    printf '%s\n' "$requested"
    return 0
  fi

  if git -C "$repo_root" rev-parse --short HEAD >/dev/null 2>&1; then
    printf '0.1.0+%s\n' "$(git -C "$repo_root" rev-parse --short HEAD)"
    return 0
  fi

  printf '0.1.0-dev\n'
}

map_machine_arch() {
  case "${1:-}" in
    x86_64|amd64)
      printf 'x64\n'
      ;;
    arm64|aarch64)
      printf 'arm64\n'
      ;;
    *)
      printf '%s\n' "${1:-unknown}"
      ;;
  esac
}

require_path() {
  local path="$1"
  local description="$2"
  if [[ ! -e "$path" ]]; then
    printf '[package][error] Missing %s: %s\n' "$description" "$path" >&2
    exit 1
  fi
}

write_json_string() {
  local value="${1//\\/\\\\}"
  value="${value//\"/\\\"}"
  value="${value//$'\n'/\\n}"
  printf '%s' "$value"
}
