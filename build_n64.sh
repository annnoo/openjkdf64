#!/usr/bin/env bash
# build_n64.sh — Build the OpenJKDF2 N64 ROM using CMake + libdragon Docker container
#
# All output goes into build_n64/ (both the cmake build tree and the final .z64).
#
# Usage:
#   ./build_n64.sh            # configure (if needed) + build  (Release)
#   ./build_n64.sh D=1        # same but with debug symbols (-g -O0) for GDB
#   ./build_n64.sh clean      # wipe build_n64/ entirely
#   ./build_n64.sh rebuild    # clean + full configure + build
#   ./build_n64.sh rebuild D=1
#   ./build_n64.sh run        # build + launch in mupen64plus
#   ./build_n64.sh configure  # cmake configure only (no build)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_n64"
ROM_OUT="$BUILD_DIR/openjkdf2.z64"

# ── Parse D=1 flag (any position in args) ─────────────────────────────────────
DEBUG_BUILD=0
ARGS=()
for arg in "$@"; do
    if [[ "$arg" == "D=1" ]]; then
        DEBUG_BUILD=1
    else
        ARGS+=("$arg")
    fi
done
set -- "${ARGS[@]+"${ARGS[@]}"}"

# ── helpers ───────────────────────────────────────────────────────────────────

info()  { printf '\033[1;34m[N64]\033[0m %s\n' "$*"; }
ok()    { printf '\033[1;32m[ OK]\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33m[WARN]\033[0m %s\n' "$*" >&2; }
err()   { printf '\033[1;31m[ERR]\033[0m %s\n' "$*" >&2; exit 1; }

check_libdragon() {
    command -v libdragon &>/dev/null \
        || err "'libdragon' not found. Install from https://github.com/DragonMinded/libdragon"
}

# Run a command inside the libdragon Docker container.
# The container mounts the project root as the working directory.
container() {
    libdragon exec sh -c "$*"
}

ensure_cmake() {
    # Check whether cmake is available inside the container.
    if ! container "cmake --version" &>/dev/null; then
        warn "cmake not found in container — installing..."
        container "apt-get update -qq && apt-get install -y --no-install-recommends cmake" \
            || err "Failed to install cmake in container"
        ok "cmake installed"
    fi
}

# ── subcommands ───────────────────────────────────────────────────────────────

cmd_configure() {
    check_libdragon
    info "Starting container..."
    libdragon start

    ensure_cmake

    local build_type="Release"
    local debug_flag="OFF"
    if [ "$DEBUG_BUILD" -eq 1 ]; then
        build_type="Debug"
        debug_flag="ON"
        info "DEBUG build (D=1): -g -O0, symbols enabled"
    fi

    info "Configuring with CMake → build_n64/..."
    mkdir -p "$BUILD_DIR"

    container "cmake \
        -S . \
        -B build_n64 \
        -DPLAT_N64=ON \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-n64.cmake \
        -DCMAKE_BUILD_TYPE=${build_type} \
        -DN64_DEBUG=${debug_flag} \
        -DCMAKE_VERBOSE_MAKEFILE=OFF"

    ok "Configure done"
}

cmd_build() {
    check_libdragon
    libdragon start

    # Re-configure only if the build directory doesn't have a CMakeCache yet
    if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        cmd_configure
    fi

    info "Building..."
    container "cmake --build build_n64 --parallel \$(nproc)"

    if [ -f "$ROM_OUT" ]; then
        SIZE=$(wc -c < "$ROM_OUT")
        ok "Built: build_n64/openjkdf2.z64  (${SIZE} bytes)"
    else
        # Some libdragon versions put the .z64 one level up — copy if so
        if [ -f "$SCRIPT_DIR/openjkdf2.z64" ]; then
            cp "$SCRIPT_DIR/openjkdf2.z64" "$ROM_OUT"
            ok "ROM copied to build_n64/openjkdf2.z64"
        else
            err "Build finished but openjkdf2.z64 not found"
        fi
    fi
}

cmd_clean() {
    info "Removing build_n64/..."
    rm -rf "$BUILD_DIR"
    ok "Clean done"
}

cmd_rebuild() {
    cmd_clean
    cmd_configure
    cmd_build
}

cmd_run() {
    cmd_build
    command -v mupen64plus &>/dev/null \
        || err "'mupen64plus' not found"
    info "Launching mupen64plus..."
    mupen64plus --noosd "$ROM_OUT"
}

# ── dispatch ──────────────────────────────────────────────────────────────────

case "${1:-build}" in
    build)     cmd_build     ;;
    configure) cmd_configure ;;
    clean)     cmd_clean     ;;
    rebuild)   cmd_rebuild   ;;
    run)       cmd_run       ;;
    *)
        printf 'Usage: %s [build|configure|clean|rebuild|run]\n' "$0"
        exit 1
        ;;
esac
