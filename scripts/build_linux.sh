#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CONFIG="Debug"
TARGET="all"
GENERATOR="auto"
TRIPLET="${VCPKG_DEFAULT_TRIPLET:-x64-linux}"
VCPKG_ROOT_INPUT="${VCPKG_ROOT:-auto}"
BUILD_ROOT="auto"
OUTPUT_ROOT="auto"
RUN_TESTS=0
SKIP_TESTS=0
CHECK_DEPS_ONLY=0
SKIP_VCPKG_INSTALL=0
BOOTSTRAP_VCPKG=0
CLEAN=0
VERBOSE=0

usage() {
  cat <<'EOF'
Usage: scripts/build_linux.sh [options]

Options:
  --config Debug|Release         Build configuration. Default: Debug
  --target all|server|linux_tray|tests
                                 Build target. Default: all
  --generator auto|ninja|make    Build generator. Default: auto
  --triplet <name>               vcpkg triplet when using vcpkg. Default: x64-linux
  --vcpkg-root <path>|auto|none  vcpkg root. Default: $VCPKG_ROOT or auto
  --build-root <path>|auto       Build root. Default: out
  --output-root <path>|auto      Staged output root. Default: out/linux/<config>
  --run-tests                    Build tests and run ctest
  --skip-tests                   Disable BUILD_TESTING
  --check-deps-only              Only check dependencies, do not configure/build
  --skip-vcpkg-install           Do not run vcpkg install even if vcpkg is used
  --bootstrap-vcpkg             Run bootstrap-vcpkg.sh when only a Windows vcpkg.exe is present
  --clean                        Remove the resolved build directory first
  --verbose                      Print executed commands
  -h, --help                     Show this help

Examples:
  scripts/build_linux.sh --target server --run-tests
  scripts/build_linux.sh --config Release --target linux_tray
  scripts/build_linux.sh --vcpkg-root /path/to/vcpkg --triplet x64-linux
EOF
}

log() {
  printf '[build_linux] %s\n' "$*"
}

warn() {
  printf '[build_linux][warn] %s\n' "$*" >&2
}

fail() {
  printf '[build_linux][error] %s\n' "$*" >&2
  exit 1
}

run_cmd() {
  if [[ "$VERBOSE" -eq 1 ]]; then
    printf '+'
    for arg in "$@"; do
      printf ' %q' "$arg"
    done
    printf '\n'
  fi
  "$@"
}

has_cmd() {
  command -v "$1" >/dev/null 2>&1
}

detect_generator() {
  if [[ "$GENERATOR" != "auto" ]]; then
    return
  fi

  if has_cmd ninja; then
    GENERATOR="ninja"
  elif has_cmd make; then
    GENERATOR="make"
  else
    fail "Neither ninja nor make is available. Install one of them."
  fi
}

resolve_build_dir() {
  if [[ "$BUILD_ROOT" == "auto" ]]; then
    BUILD_ROOT="$ROOT_DIR/out"
  fi

  BUILD_DIR="$BUILD_ROOT/build/linux-${GENERATOR}-${CONFIG}"
}

resolve_output_dir() {
  if [[ "$OUTPUT_ROOT" == "auto" ]]; then
    OUTPUT_DIR="$ROOT_DIR/out/linux/${CONFIG}"
  else
    OUTPUT_DIR="$OUTPUT_ROOT"
  fi
}

find_vcpkg_root() {
  local candidates=()

  if [[ "$VCPKG_ROOT_INPUT" != "auto" && "$VCPKG_ROOT_INPUT" != "none" ]]; then
    candidates+=("$VCPKG_ROOT_INPUT")
  fi

  if [[ "${VCPKG_ROOT:-}" != "" ]]; then
    candidates+=("$VCPKG_ROOT")
  fi

  candidates+=(
    "$HOME/vcpkg"
    "/opt/vcpkg"
    "/usr/local/share/vcpkg"
    "/mnt/c/vcpkg"
    "/mnt/c/dev/vcpkg"
  )

  for candidate in "${candidates[@]}"; do
    [[ -n "$candidate" ]] || continue
    if [[ -d "$candidate" && -f "$candidate/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

resolve_vcpkg_executable() {
  local root="$1"
  local candidates=(
    "$root/vcpkg"
    "$root/vcpkg.exe"
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -f "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

is_windows_vcpkg_executable() {
  local exe="$1"
  [[ "$exe" == *.exe ]]
}

needs_native_vcpkg_bootstrap() {
  if [[ -z "${RESOLVED_VCPKG_ROOT:-}" ]]; then
    return 1
  fi

  if [[ -z "${RESOLVED_VCPKG_EXE:-}" ]]; then
    return 0
  fi

  is_windows_vcpkg_executable "$RESOLVED_VCPKG_EXE"
}

check_shared_lib() {
  local pattern="$1"
  if has_cmd ldconfig && ldconfig -p 2>/dev/null | grep -F "$pattern" >/dev/null 2>&1; then
    return 0
  fi
  return 1
}

check_dependencies() {
  local using_vcpkg=0
  local compiler_name=""
  local need_bootstrap_deps=0

  log "Checking Linux build dependencies"

  has_cmd cmake || fail "cmake is required."
  has_cmd ctest || warn "ctest is not in PATH. Test execution will fail unless CMake provides it."

  if [[ -n "${CXX:-}" ]]; then
    has_cmd "$CXX" || fail "CXX is set to '$CXX' but that compiler is not in PATH."
    compiler_name="$CXX"
  elif has_cmd g++; then
    compiler_name="g++"
  elif has_cmd clang++; then
    compiler_name="clang++"
  else
    fail "No C++ compiler found. Install g++ or clang++."
  fi
  log "Compiler: $compiler_name"

  detect_generator
  if [[ "$GENERATOR" == "ninja" ]]; then
    has_cmd ninja || fail "Generator 'ninja' was selected but ninja is not installed."
  else
    has_cmd make || fail "Generator 'make' was selected but make is not installed."
  fi
  log "Generator: $GENERATOR"

  RESOLVED_VCPKG_ROOT=""
  RESOLVED_VCPKG_EXE=""
  if [[ "$VCPKG_ROOT_INPUT" != "none" ]] && RESOLVED_VCPKG_ROOT="$(find_vcpkg_root)"; then
    using_vcpkg=1
    if [[ "$SKIP_VCPKG_INSTALL" -eq 0 ]]; then
      RESOLVED_VCPKG_EXE="$(resolve_vcpkg_executable "$RESOLVED_VCPKG_ROOT" || true)"
      if needs_native_vcpkg_bootstrap; then
        if [[ "$BOOTSTRAP_VCPKG" -eq 1 ]]; then
          [[ -f "$RESOLVED_VCPKG_ROOT/bootstrap-vcpkg.sh" ]] || \
            fail "bootstrap-vcpkg.sh was not found in '$RESOLVED_VCPKG_ROOT'."
          need_bootstrap_deps=1
          warn "A native Linux vcpkg binary is not available yet. The script will run bootstrap-vcpkg.sh before installing dependencies."
        elif [[ -z "$RESOLVED_VCPKG_EXE" ]]; then
          fail "vcpkg root was found at '$RESOLVED_VCPKG_ROOT' but no vcpkg executable was found. Rerun with --bootstrap-vcpkg."
        else
          fail "Detected Windows vcpkg executable '$RESOLVED_VCPKG_EXE'. Linux dependency installation needs a native vcpkg binary. Run '$RESOLVED_VCPKG_ROOT/bootstrap-vcpkg.sh' first, or rerun with --bootstrap-vcpkg."
        fi
      fi
    fi
    log "vcpkg: $RESOLVED_VCPKG_ROOT"
  else
    warn "vcpkg toolchain not found. The build will rely on system-installed CMake package configs for Boost/OpenSSL."
    warn "If configure fails on Boost::*/OpenSSL::*, set --vcpkg-root <path> or export CMAKE_PREFIX_PATH."
  fi

  if [[ "$need_bootstrap_deps" -eq 1 ]]; then
    has_cmd curl || fail "bootstrap-vcpkg.sh requires curl."
    has_cmd zip || fail "bootstrap-vcpkg.sh requires zip."
    has_cmd unzip || fail "bootstrap-vcpkg.sh requires unzip."
    has_cmd tar || fail "bootstrap-vcpkg.sh requires tar."
    has_cmd git || fail "bootstrap-vcpkg.sh requires git."
  fi

  if [[ "$TARGET" == "all" || "$TARGET" == "linux_tray" ]]; then
    if has_cmd xdg-open; then
      log "Found xdg-open"
    else
      warn "xdg-open not found. Open Dashboard/Viewer/Diagnostics actions will fail at runtime."
    fi

    if has_cmd notify-send; then
      log "Found notify-send"
    else
      warn "notify-send not found. Native notifications will fail at runtime."
    fi

    if check_shared_lib "libgtk-3.so.0"; then
      log "Found GTK3 runtime"
    else
      warn "libgtk-3.so.0 not found via ldconfig. lan_screenshare_linux_tray will not start without GTK3 runtime."
    fi

    if check_shared_lib "libayatana-appindicator3.so.1" || check_shared_lib "libappindicator3.so.1"; then
      log "Found AppIndicator runtime"
    else
      warn "AppIndicator runtime not found via ldconfig. lan_screenshare_linux_tray will not start without it."
    fi
  fi

  if [[ "$using_vcpkg" -eq 1 ]]; then
    log "Dependency check complete: build will use vcpkg"
  else
    log "Dependency check complete: build will use system packages/CMAKE_PREFIX_PATH"
  fi
}

ensure_native_vcpkg() {
  [[ -n "${RESOLVED_VCPKG_ROOT:-}" ]] || return 0
  [[ "$SKIP_VCPKG_INSTALL" -eq 0 ]] || return 0

  RESOLVED_VCPKG_EXE="$(resolve_vcpkg_executable "$RESOLVED_VCPKG_ROOT" || true)"
  if ! needs_native_vcpkg_bootstrap; then
    return 0
  fi

  [[ "$BOOTSTRAP_VCPKG" -eq 1 ]] || fail "A native Linux vcpkg binary is required. Rerun with --bootstrap-vcpkg."
  [[ -f "$RESOLVED_VCPKG_ROOT/bootstrap-vcpkg.sh" ]] || fail "bootstrap-vcpkg.sh was not found in '$RESOLVED_VCPKG_ROOT'."

  log "Bootstrapping native vcpkg in $RESOLVED_VCPKG_ROOT"
  (
    cd "$RESOLVED_VCPKG_ROOT"
    run_cmd ./bootstrap-vcpkg.sh
  )

  RESOLVED_VCPKG_EXE="$(resolve_vcpkg_executable "$RESOLVED_VCPKG_ROOT" || true)"
  [[ -n "$RESOLVED_VCPKG_EXE" ]] || fail "bootstrap-vcpkg.sh finished but no vcpkg executable was produced."
  is_windows_vcpkg_executable "$RESOLVED_VCPKG_EXE" && \
    fail "bootstrap-vcpkg.sh did not produce a native Linux vcpkg binary."
}

configure_cmake() {
  mkdir -p "$BUILD_DIR"

  local -a args=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -Wno-dev
    "-DBUILD_TESTING=$( [[ "$SKIP_TESTS" -eq 1 ]] && printf OFF || printf ON )"
  )

  if [[ "$GENERATOR" == "ninja" ]]; then
    args+=(-G Ninja)
  else
    args+=(-G "Unix Makefiles")
  fi

  if [[ "$CONFIG" == "Debug" || "$CONFIG" == "Release" ]]; then
    args+=("-DCMAKE_BUILD_TYPE=$CONFIG")
  fi

  if [[ -n "${CXX:-}" ]]; then
    args+=("-DCMAKE_CXX_COMPILER=$CXX")
  fi

  if [[ -n "${CC:-}" ]]; then
    args+=("-DCMAKE_C_COMPILER=$CC")
  fi

  if [[ -n "${RESOLVED_VCPKG_ROOT:-}" ]]; then
    args+=(
      "-DCMAKE_TOOLCHAIN_FILE=$RESOLVED_VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
      "-DVCPKG_TARGET_TRIPLET=$TRIPLET"
    )
  fi

  log "Configuring CMake in $BUILD_DIR"
  run_cmd cmake "${args[@]}"
}

run_vcpkg_install() {
  [[ -n "${RESOLVED_VCPKG_ROOT:-}" ]] || return 0
  [[ "$SKIP_VCPKG_INSTALL" -eq 0 ]] || return 0

  ensure_native_vcpkg
  log "Installing vcpkg dependencies for triplet $TRIPLET"
  run_cmd "$RESOLVED_VCPKG_EXE" install --triplet "$TRIPLET"
}

build_target() {
  local cmake_target="$1"
  local -a args=(--build "$BUILD_DIR")

  if [[ -n "$cmake_target" ]]; then
    args+=(--target "$cmake_target")
  fi

  log "Building ${cmake_target:-default target}"
  run_cmd cmake "${args[@]}"
}

stage_file() {
  local source="$1"
  local dest_dir="$2"
  mkdir -p "$dest_dir"
  cp -f "$source" "$dest_dir/"
}

stage_tree() {
  local source_dir="$1"
  local dest_dir="$2"
  mkdir -p "$dest_dir"
  rm -rf "$dest_dir"
  mkdir -p "$dest_dir"
  cp -R "$source_dir/." "$dest_dir/"
}

stage_outputs() {
  resolve_output_dir
  mkdir -p "$OUTPUT_DIR"

  if [[ "$TARGET" == "all" || "$TARGET" == "server" || "$TARGET" == "tests" ]]; then
    local server_bin="$BUILD_DIR/ViewMeshServer"
    if [[ -f "$server_bin" ]]; then
      local server_out="$OUTPUT_DIR/server"
      stage_file "$server_bin" "$server_out"
      stage_tree "$ROOT_DIR/www" "$server_out/www"
      stage_tree "$ROOT_DIR/src/desktop_host/webui" "$server_out/webui"
      log "Staged server output to $server_out"
    fi
  fi

  if [[ "$TARGET" == "all" || "$TARGET" == "linux_tray" ]]; then
    local tray_bin="$BUILD_DIR/lan_screenshare_linux_tray"
    if [[ -f "$tray_bin" ]]; then
      local tray_out="$OUTPUT_DIR/tray"
      stage_file "$tray_bin" "$tray_out"
      stage_tree "$ROOT_DIR/src/resources/icons/linux" "$tray_out/icons/linux"
      log "Staged tray output to $tray_out"
    fi
  fi
}

run_tests() {
  [[ "$SKIP_TESTS" -eq 0 ]] || fail "--run-tests and --skip-tests cannot be used together."

  if [[ "$TARGET" == "server" || "$TARGET" == "linux_tray" ]]; then
    log "Building default target so test executables exist"
    build_target ""
  fi

  log "Running ctest"
  run_cmd ctest --test-dir "$BUILD_DIR" --output-on-failure
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --config)
        [[ $# -ge 2 ]] || fail "--config requires a value"
        CONFIG="$2"
        shift 2
        ;;
      --target)
        [[ $# -ge 2 ]] || fail "--target requires a value"
        TARGET="$2"
        shift 2
        ;;
      --generator)
        [[ $# -ge 2 ]] || fail "--generator requires a value"
        GENERATOR="$2"
        shift 2
        ;;
      --triplet)
        [[ $# -ge 2 ]] || fail "--triplet requires a value"
        TRIPLET="$2"
        shift 2
        ;;
      --vcpkg-root)
        [[ $# -ge 2 ]] || fail "--vcpkg-root requires a value"
        VCPKG_ROOT_INPUT="$2"
        shift 2
        ;;
      --build-root)
        [[ $# -ge 2 ]] || fail "--build-root requires a value"
        BUILD_ROOT="$2"
        shift 2
        ;;
      --output-root)
        [[ $# -ge 2 ]] || fail "--output-root requires a value"
        OUTPUT_ROOT="$2"
        shift 2
        ;;
      --run-tests)
        RUN_TESTS=1
        shift
        ;;
      --skip-tests)
        SKIP_TESTS=1
        shift
        ;;
      --check-deps-only)
        CHECK_DEPS_ONLY=1
        shift
        ;;
      --skip-vcpkg-install)
        SKIP_VCPKG_INSTALL=1
        shift
        ;;
      --bootstrap-vcpkg)
        BOOTSTRAP_VCPKG=1
        shift
        ;;
      --clean)
        CLEAN=1
        shift
        ;;
      --verbose)
        VERBOSE=1
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        fail "Unknown argument: $1"
        ;;
    esac
  done

  case "$CONFIG" in
    Debug|Release) ;;
    *) fail "Unsupported --config value: $CONFIG" ;;
  esac

  case "$TARGET" in
    all|server|linux_tray|tests) ;;
    *) fail "Unsupported --target value: $TARGET" ;;
  esac

  case "$GENERATOR" in
    auto|ninja|make) ;;
    *) fail "Unsupported --generator value: $GENERATOR" ;;
  esac
}

main() {
  parse_args "$@"
  check_dependencies

  if [[ "$CHECK_DEPS_ONLY" -eq 1 ]]; then
    return 0
  fi

  resolve_build_dir
  if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
    log "Removing build directory $BUILD_DIR"
    rm -rf "$BUILD_DIR"
  fi

  run_vcpkg_install
  configure_cmake

  case "$TARGET" in
    all)
      build_target ""
      ;;
    server)
      build_target "lan_screenshare_server"
      ;;
    linux_tray)
      build_target "lan_screenshare_linux_tray"
      ;;
    tests)
      [[ "$SKIP_TESTS" -eq 0 ]] || fail "--target tests requires tests to be enabled"
      build_target ""
      RUN_TESTS=1
      ;;
  esac

  stage_outputs

  if [[ "$RUN_TESTS" -eq 1 ]]; then
    run_tests
  fi

  log "Done"
}

main "$@"
