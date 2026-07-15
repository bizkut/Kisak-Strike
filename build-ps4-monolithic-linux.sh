#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${KISAK_PS4_ENGINE_BUILD_DIR:-$ROOT_DIR/build-ps4-engine}"
OPENGNM_ROOT="${KISAK_OPENGNM_ROOT:-$ROOT_DIR/../OpenGNM}"

if [[ -z "${OO_PS4_TOOLCHAIN:-}" ]]; then
    SETUP="$ROOT_DIR/../tools/setup_openorbis_llvm18_linux.sh"
    OO_PS4_TOOLCHAIN="$($SETUP)"
    export OO_PS4_TOOLCHAIN
fi
if [[ -z "${LLVM18_PREFIX:-}" ]]; then
    if command -v llvm-config-18 >/dev/null 2>&1; then
        LLVM18_PREFIX="$(llvm-config-18 --prefix)"
    elif [[ -d /usr/lib/llvm-18 ]]; then
        LLVM18_PREFIX="/usr/lib/llvm-18"
    fi
    export LLVM18_PREFIX
fi
if [[ -n "${LLVM18_PREFIX:-}" ]]; then
    export PATH="$LLVM18_PREFIX/bin:$PATH"
fi

if [[ "${KISAK_OPENGNM_BUILD:-1}" == "1" ]]; then
    if [[ ! -f "$OPENGNM_ROOT/Makefile" ]]; then
        echo "OpenGNM source tree not found at $OPENGNM_ROOT" >&2
        echo "Set KISAK_OPENGNM_BUILD=0 to use an intentional prebuilt archive." >&2
        exit 1
    fi
    if [[ -f "$OPENGNM_ROOT/config.mak" ]]; then
        "$OPENGNM_ROOT/build.sh" clean
    fi
    "$OPENGNM_ROOT/build.sh" lib
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/OpenOrbis.cmake" \
    -DKISAK_OPENGNM_ROOT="$OPENGNM_ROOT" \
    -DKISAK_CREATE_FSELF="$OO_PS4_TOOLCHAIN/bin/linux/create-fself" \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" --target kisak_ps4_monolithic --parallel

echo "Kisak PS4 monolithic artifacts: $BUILD_DIR"
