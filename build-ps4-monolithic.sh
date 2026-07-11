#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${KISAK_PS4_ENGINE_BUILD_DIR:-$ROOT_DIR/build-ps4-engine}"

if [[ -z "${OO_PS4_TOOLCHAIN:-}" ]]; then
    SETUP="$ROOT_DIR/../tools/setup_openorbis_llvm18_macos.sh"
    OO_PS4_TOOLCHAIN="$($SETUP)"
    export OO_PS4_TOOLCHAIN
fi
if [[ -z "${LLVM18_PREFIX:-}" ]] && command -v brew >/dev/null 2>&1; then
    LLVM18_PREFIX="$(brew --prefix llvm@18)"
    export LLVM18_PREFIX
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/OpenOrbis.cmake" \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" --target kisak_ps4_monolithic --parallel

echo "Kisak PS4 monolithic artifacts: $BUILD_DIR"
