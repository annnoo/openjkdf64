#!/usr/bin/env bash
# build_n64.sh — Build the OpenJKDF2 N64 ROM using CMake + libdragon Docker container
#
# All output goes into build_n64/ (both the cmake build tree and the final .z64).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_n64"
ROM_OUT="$BUILD_DIR/openjkdf2.z64"

# ── Parse D=1 flag ────────────────────────────────────────────────────────────
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

container() {
    libdragon exec sh -c "$*"
}

ensure_cmake() {
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

cmd_sync_assets() {
    local ASSET_DIR="$SCRIPT_DIR/tools/out"
    local FS_DIR="$SCRIPT_DIR/filesystem"

    info "Clearing filesystem directory..."
    rm -rf "$FS_DIR"
    mkdir -p "$FS_DIR"

    info "Syncing surgical Level 1 assets to root..."
    
    # 1. Sync core directories fully (needed for GUI and game logic)
    for subdir in ui misc cog; do
        if [ -d "$ASSET_DIR/$subdir" ]; then
            cp -ru "$ASSET_DIR/$subdir" "$FS_DIR/"
        fi
    done

    # 2. Add CD bypass
    touch "$FS_DIR/jk_.cd"

    # 3. Setup Episode file for discovery
    local EP_DIR="$FS_DIR/episode/jk1"
    mkdir -p "$EP_DIR"
    if [ -f "$SCRIPT_DIR/tools/out_level1/episode.jk" ]; then
        cp "$SCRIPT_DIR/tools/out_level1/episode.jk" "$EP_DIR/"
    elif [ -f "$ASSET_DIR/episode.jk" ]; then
        cp "$ASSET_DIR/episode.jk" "$EP_DIR/"
    fi

    # 4. Level 1 Surgical Sync (3DO, MAT, JKL) to root
    local JKL_PATH=""
    if [ -f "$SCRIPT_DIR/tools/out_level1/jkl/01narshadda.jkl" ]; then
        JKL_PATH="$SCRIPT_DIR/tools/out_level1/jkl/01narshadda.jkl"
    elif [ -f "$ASSET_DIR/jkl/01narshadda.jkl" ]; then
        JKL_PATH="$ASSET_DIR/jkl/01narshadda.jkl"
    fi

    if [ -n "$JKL_PATH" ]; then
        mkdir -p "$FS_DIR/jkl"
        cp "$JKL_PATH" "$FS_DIR/jkl/"
        # Run surgical filter for 3do/mat, outputting to FS root
        python3 tools/filter_level_assets.py "$JKL_PATH" "$ASSET_DIR" "$FS_DIR"
    else
        warn "01narshadda.jkl not found"
    fi

    ok "Assets synced to root (Filtered)"
}

cmd_build() {
    check_libdragon
    libdragon start

    if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        cmd_configure
    fi

    # Sync assets before building DFS
    cmd_sync_assets

    info "Building..."
    container "cmake --build build_n64 --parallel 4"

    if [ -f "$ROM_OUT" ]; then
        SIZE=$(wc -c < "$ROM_OUT")
        ok "Built: build_n64/openjkdf2.z64  (${SIZE} bytes)"
    else
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
    info "Launching..."
    ares --noosd "$ROM_OUT"
}

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
