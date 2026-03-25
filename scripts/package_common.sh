#!/usr/bin/env bash

set -euo pipefail

package_repo_root() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  cd "$script_dir/.." && pwd
}

package_fail() {
  local scope="$1"
  shift
  printf '[%s][error] %s\n' "$scope" "$*" >&2
  exit 1
}

require_option_value() {
  local scope="$1"
  local flag="$2"
  local value="${3-}"
  [[ -n "$value" ]] || package_fail "$scope" "$flag requires a value"
}

validate_package_config() {
  local scope="$1"
  local config="$2"
  case "$config" in
    Debug|Release) ;;
    *)
      package_fail "$scope" "Unsupported config: $config"
      ;;
  esac
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

resolve_vcpkg_executable() {
  local root="$1"
  local candidate
  for candidate in "$root/vcpkg" "$root/vcpkg.exe"; do
    if [[ -x "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  return 1
}

require_vcpkg_root() {
  local scope="$1"
  local root="${2:-}"
  [[ -n "$root" ]] || package_fail "$scope" "--vcpkg-root (or VCPKG_ROOT) is required when building."
  [[ -d "$root" ]] || package_fail "$scope" "vcpkg root not found: $root"
  [[ -f "$root/scripts/buildsystems/vcpkg.cmake" ]] || \
    package_fail "$scope" "vcpkg toolchain not found under $root"
}

ensure_vcpkg_executable() {
  local scope="$1"
  local root="$2"
  local bootstrap="${3:-0}"
  local exe=""

  exe="$(resolve_vcpkg_executable "$root" || true)"
  if [[ -z "$exe" && "$bootstrap" -eq 1 ]]; then
    [[ -x "$root/bootstrap-vcpkg.sh" ]] || package_fail "$scope" "bootstrap-vcpkg.sh not found under $root"
    (cd "$root" && ./bootstrap-vcpkg.sh -disableMetrics)
    exe="$(resolve_vcpkg_executable "$root" || true)"
  fi

  [[ -n "$exe" ]] || package_fail "$scope" "vcpkg executable not found under $root"
  printf '%s\n' "$exe"
}

print_package_result() {
  local label="$1"
  local path="$2"
  printf '%-9s %s\n' "${label}:" "$path"
}
