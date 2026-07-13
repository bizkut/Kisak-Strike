#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHADER_DIR="$ROOT_DIR/ps4/shaders"
GLSLC="${GLSLC:-$(command -v glslc || true)}"
PSBC="${PSBC:-$ROOT_DIR/../psbc/psbc}"

if [[ -z "$GLSLC" || ! -x "$GLSLC" ]]; then
    echo "Missing glslc; set GLSLC to the Vulkan shader compiler." >&2
    exit 1
fi
if [[ ! -x "$PSBC" ]]; then
    echo "Missing native PS4 psbc compiler: $PSBC" >&2
    exit 1
fi

compile_shader() {
    local stage="$1"
    local source="$2"
    local stem="${source%.glsl}"
    "$GLSLC" -fshader-stage="$stage" "$SHADER_DIR/$source" \
        -o "$SHADER_DIR/$stem.spv"
    "$PSBC" -s "$stage" -f "$SHADER_DIR/$stem.spv" \
        -o "$SHADER_DIR/$stem.sb"
}

compile_shader vertex kisak_scaleform_ordered.vert.glsl
compile_shader fragment kisak_scaleform_ordered.frag.glsl

echo "Kisak PS4 shaders built in $SHADER_DIR"
