#!/bin/bash

# ========== CONFIGURATION ==========
EXECUTABLE=module-rtmp
TOOLCHAIN=toolchain-mc6630.cmake
USE_TOOLCHAIN=true
# ===================================

ROOT_DIR="$(realpath "$(dirname "$0")")"
BUILD_DIR="$ROOT_DIR/built"
TOOLCHAIN_PATH="$ROOT_DIR/$TOOLCHAIN"

echo "📁 ROOT_DIR: $ROOT_DIR"
echo "🔧 BUILD_PATH: $BUILD_DIR"

function generate() {
    echo "🔧 Generating CMake files..."
    mkdir -p "$BUILD_DIR"
    if [ "$USE_TOOLCHAIN" = true ]; then
        #cp -f $ROOT_DIR/CMakeLists.txt $BUILD_DIR
        cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_PATH"
    else
        cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
    fi
}

function build() {
    echo "⚙️ Building..."
    cmake --build "$BUILD_DIR"
}

function run() {
    echo "🚀 Running $EXECUTABLE"
    "$BUILD_DIR/$EXECUTABLE"
}

function clean() {
    echo "🧹 Cleaning build..."
    rm -rf "$BUILD_DIR"
}

function help() {
    echo "Usage: ./build.sh [build|rebuild|clean|run|help]"
}

case "$1" in
    build)
        generate && build ;;
    rebuild)
        clean && generate && build ;;
    clean)
        clean ;;
    run)
        run ;;
    help|*)
        help ;;
esac
