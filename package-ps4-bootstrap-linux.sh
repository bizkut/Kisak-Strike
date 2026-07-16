#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VARIANT="${KISAK_PS4_VARIANT:-bootstrap}"
CONTENT_PROBE="$ROOT_DIR/ps4/content/kisak_ps4_content_probe.txt"
DIAGNOSTIC_SHADER_DIR="${KISAK_PS4_DIAGNOSTIC_SHADER_DIR:-$ROOT_DIR/../freegnm-examples/eden-renderer-draw/assets/misc}"
CLEAR_SHADER="${KISAK_PS4_CLEAR_SHADER:-$ROOT_DIR/../freegnm-examples/cube/assets/misc/clear.frag.sb}"
CUBE_SHADER_DIR="${KISAK_PS4_CUBE_SHADER_DIR:-$ROOT_DIR/../freegnm-examples/cube/assets/misc}"
REFERENCE_TEXTURE_PIXEL_SHADER="${KISAK_PS4_REFERENCE_TEXTURE_PIXEL_SHADER:-$ROOT_DIR/../freegnm-examples/eden-composite-blit/assets/misc/blit.frag.sb}"
SOLID_COLOR_PIXEL_SHADER="${KISAK_PS4_SOLID_COLOR_PIXEL_SHADER:-$ROOT_DIR/../freegnm-examples/triangle/assets/misc/tri.frag.sb}"
if [[ "$VARIANT" == "monolithic" ]]; then
    BUILD_DIR="${KISAK_PS4_ENGINE_BUILD_DIR:-$ROOT_DIR/build-ps4-engine}"
    TITLE="Kisak-Strike PS4 Monolithic"
    VERSION="3.28"
    TITLE_ID="KISK00002"
    CONTENT_ID="IV0000-KISK00002_00-KISAKMONOLITHIC0"
    EBOOT_INPUT="$BUILD_DIR/kisak_ps4_monolithic.bin"
else
    BUILD_DIR="${KISAK_PS4_BUILD_DIR:-$ROOT_DIR/build-ps4-bootstrap}"
    TITLE="Kisak-Strike PS4 Bootstrap"
    VERSION="1.01"
    TITLE_ID="KISK00001"
    CONTENT_ID="IV0000-KISK00001_00-KISAKBOOTSTRAP00"
    EBOOT_INPUT="$BUILD_DIR/eboot.bin"
fi
PACKAGE_DIR="$BUILD_DIR/package"
DOTNET_BUNDLE_EXTRACT_BASE_DIR="${DOTNET_BUNDLE_EXTRACT_BASE_DIR:-$BUILD_DIR/dotnet-bundle}"
export DOTNET_BUNDLE_EXTRACT_BASE_DIR
DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="${DOTNET_SYSTEM_GLOBALIZATION_INVARIANT:-1}"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT
OPENORBIS_LIBSSL11_DIR="${OPENORBIS_LIBSSL11_DIR:-$ROOT_DIR/../.host-tools/libssl11/root/usr/lib/x86_64-linux-gnu}"
if [[ -d "$OPENORBIS_LIBSSL11_DIR" ]]; then
    LD_LIBRARY_PATH="$OPENORBIS_LIBSSL11_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
fi
PACKAGE="$CONTENT_ID.pkg"

if [[ -z "${OO_PS4_TOOLCHAIN:-}" ]]; then
    SETUP="$ROOT_DIR/../tools/setup_openorbis_llvm18_linux.sh"
    if [[ ! -x "$SETUP" ]]; then
        echo "Set OO_PS4_TOOLCHAIN to the OpenOrbis SDK root." >&2
        exit 1
    fi
    OO_PS4_TOOLCHAIN="$($SETUP)"
    export OO_PS4_TOOLCHAIN
fi

if [[ "$VARIANT" == "monolithic" ]]; then
    KISAK_PS4_DEV_ATTACH_GATE=0 "$ROOT_DIR/build-ps4-monolithic-linux.sh"
else
    "$ROOT_DIR/build-ps4-bootstrap-linux.sh"
fi

CREATE_GP4="${CREATE_GP4:-$OO_PS4_TOOLCHAIN/bin/linux/create-gp4}"
PKGTOOL="${PKGTOOL:-$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core}"
RUNTIME_MODULE_DIR="${RUNTIME_MODULE_DIR:-$OO_PS4_TOOLCHAIN/src/modules}"
ASSET_DIR="${KISAK_PS4_ASSET_DIR:-$ROOT_DIR/../freegnm-examples/videoout-linear/sce_sys}"
ICON_PATH="${KISAK_PS4_ICON_PATH:-$ROOT_DIR/ps4/sce_sys/icon0.png}"
SHADER_MANIFEST="${KISAK_PS4_SHADER_MANIFEST:-$ROOT_DIR/ps4/shaders/kisak_diagnostic.manifest}"
SCALEFORM_ASSET_ROOT="${KISAK_PS4_SCALEFORM_ASSET_ROOT:-/home/bizkut/CSGO/reversed/extracted_FIX/NPUB30589_00-HDDBOOTCSGO00001/USRDIR/csgo/zip1_xzp2}"
SCALEFORM_ASSET_MODE="${KISAK_PS4_SCALEFORM_ASSET_MODE:-closure}"
PYTHON3="${PYTHON3:-python3}"
SCALEFORM_FLASH_FILES=(
    fontmapping.cfg
    fontlib.gfx
    fontlib_extra.swf
    sharedlib.gfx
    colorlib.gfx
    mainmenu.gfx
    mainuirootmovie.gfx
    pausemenu.gfx
    scoreboard.gfx
)

for required in "$CREATE_GP4" "$PKGTOOL" "$RUNTIME_MODULE_DIR/libc.prx" \
    "$RUNTIME_MODULE_DIR/libSceFios2.prx" "$ICON_PATH" \
    "$ASSET_DIR/about/right.sprx" "$EBOOT_INPUT" "$SHADER_MANIFEST"; do
    if [[ ! -e "$required" ]]; then
        echo "Missing PS4 package input: $required" >&2
        exit 1
    fi
done
if [[ "$VARIANT" == "monolithic" && ! -f "$CONTENT_PROBE" ]]; then
    echo "Missing PS4 package input: $CONTENT_PROBE" >&2
    exit 1
fi
if [[ "$VARIANT" == "monolithic" ]]; then
    if [[ ! -d "$SCALEFORM_ASSET_ROOT/resource/flash" ]]; then
        echo "Missing Scaleform asset root: $SCALEFORM_ASSET_ROOT/resource/flash" >&2
        exit 1
    fi
    if [[ "$SCALEFORM_ASSET_MODE" == "closure" || "$SCALEFORM_ASSET_MODE" == "all" ]]; then
        SCALEFORM_FLASH_FILES=()
        while IFS= read -r asset_path; do
            SCALEFORM_FLASH_FILES+=( "${asset_path##*/}" )
        done < <(
            if [[ "$SCALEFORM_ASSET_MODE" == "all" ]]; then
                find "$SCALEFORM_ASSET_ROOT/resource/flash" -maxdepth 1 -type f -print
            else
                find "$SCALEFORM_ASSET_ROOT/resource/flash" -maxdepth 1 -type f \
                    \( -name '*.gfx' -o -name '*.swf' -o \
                       \( -name '*.dds' ! -name '* *' \) -o \
                       -name 'fontmapping.cfg' \) -print
            fi | sort
        )
    elif [[ "$SCALEFORM_ASSET_MODE" != "roots" ]]; then
        echo "Unsupported Scaleform asset mode: $SCALEFORM_ASSET_MODE" >&2
        exit 1
    fi
    for flash in "${SCALEFORM_FLASH_FILES[@]}"; do
        if [[ ! -f "$SCALEFORM_ASSET_ROOT/resource/flash/$flash" ]]; then
            echo "Missing Scaleform movie asset: $SCALEFORM_ASSET_ROOT/resource/flash/$flash" >&2
            exit 1
        fi
    done
    if ! command -v "$PYTHON3" >/dev/null 2>&1; then
        echo "Missing Python 3 interpreter for Scaleform SWF preparation: $PYTHON3" >&2
        exit 1
    fi
fi
if [[ "$VARIANT" == "monolithic" ]]; then
    for shader in tri.vert.sb texture_sample.frag.sb present.frag.sb; do
        if [[ ! -f "$DIAGNOSTIC_SHADER_DIR/$shader" ]]; then
            echo "Missing generated PS4 diagnostic shader: $DIAGNOSTIC_SHADER_DIR/$shader" >&2
            exit 1
        fi
    done
    if [[ ! -f "$SOLID_COLOR_PIXEL_SHADER" ]]; then
        echo "Missing pass-through solid-color pixel shader: $SOLID_COLOR_PIXEL_SHADER" >&2
        exit 1
    fi
    if [[ ! -f "$CLEAR_SHADER" ]]; then
        echo "Missing PS4 depth clear shader: $CLEAR_SHADER" >&2
        exit 1
    fi
    for shader in cube.vert.sb cube.frag.sb; do
        if [[ ! -f "$CUBE_SHADER_DIR/$shader" ]]; then
            echo "Missing reference cube shader: $CUBE_SHADER_DIR/$shader" >&2
            exit 1
        fi
    done
    if [[ ! -f "$REFERENCE_TEXTURE_PIXEL_SHADER" ]]; then
        echo "Missing RGBA texture pixel shader: $REFERENCE_TEXTURE_PIXEL_SHADER" >&2
        exit 1
    fi
    for shader in kisak_scaleform_ordered.vert.sb kisak_scaleform_ordered.frag.sb; do
        if [[ ! -f "$ROOT_DIR/ps4/shaders/$shader" ]]; then
            echo "Missing Kisak ordered Scaleform shader: $ROOT_DIR/ps4/shaders/$shader" >&2
            exit 1
        fi
    done
fi

rm -rf "$PACKAGE_DIR"
mkdir -p "$DOTNET_BUNDLE_EXTRACT_BASE_DIR" "$PACKAGE_DIR/sce_module" "$PACKAGE_DIR/sce_sys/about"
cp "$EBOOT_INPUT" "$PACKAGE_DIR/eboot.bin"
cp "$RUNTIME_MODULE_DIR/libc.prx" "$PACKAGE_DIR/sce_module/libc.prx"
cp "$RUNTIME_MODULE_DIR/libSceFios2.prx" "$PACKAGE_DIR/sce_module/libSceFios2.prx"
cp "$ICON_PATH" "$PACKAGE_DIR/sce_sys/icon0.png"
cp "$ASSET_DIR/about/right.sprx" "$PACKAGE_DIR/sce_sys/about/right.sprx"
if [[ "$VARIANT" == "monolithic" ]]; then
    cp "$CONTENT_PROBE" "$PACKAGE_DIR/kisak_ps4_content_probe.txt"
    mkdir -p "$PACKAGE_DIR/resource/flash"
    for flash in "${SCALEFORM_FLASH_FILES[@]}"; do
        cp "$SCALEFORM_ASSET_ROOT/resource/flash/$flash" "$PACKAGE_DIR/resource/flash/$flash"
    done
    "$PYTHON3" "$ROOT_DIR/ps4/prepare_scaleform_swf.py" "$PACKAGE_DIR/resource/flash"
    cp "$DIAGNOSTIC_SHADER_DIR/tri.vert.sb" "$PACKAGE_DIR/kisak_diagnostic.vert.sb"
    cp "$SOLID_COLOR_PIXEL_SHADER" "$PACKAGE_DIR/kisak_diagnostic.frag.sb"
    cp "$DIAGNOSTIC_SHADER_DIR/texture_sample.frag.sb" "$PACKAGE_DIR/kisak_texture_sample.frag.sb"
    cp "$DIAGNOSTIC_SHADER_DIR/present.frag.sb" "$PACKAGE_DIR/kisak_present.frag.sb"
    cp "$CLEAR_SHADER" "$PACKAGE_DIR/kisak_depth_clear.frag.sb"
    cp "$CUBE_SHADER_DIR/cube.vert.sb" "$PACKAGE_DIR/kisak_reference_cube.vert.sb"
    cp "$REFERENCE_TEXTURE_PIXEL_SHADER" "$PACKAGE_DIR/kisak_reference_cube.frag.sb"
    cp "$ROOT_DIR/ps4/shaders/kisak_scaleform_ordered.vert.sb" \
        "$PACKAGE_DIR/kisak_scaleform_ordered.vert.sb"
    cp "$ROOT_DIR/ps4/shaders/kisak_scaleform_ordered.frag.sb" \
        "$PACKAGE_DIR/kisak_scaleform_ordered.frag.sb"
    cp "$SHADER_MANIFEST" "$PACKAGE_DIR/kisak_diagnostic.manifest"

    manifest_entries=0
    manifest_keys=""
    while IFS='|' read -r shader_name shader_stage static_combo dynamic_combo vertex_format shader_path vertex_inputs constant_bytes sampler_mask fragment_outputs; do
        [[ -z "$shader_name" || "$shader_name" == \#* ]] && continue
        if [[ "$shader_stage" != "vertex" && "$shader_stage" != "pixel" ]]; then
            echo "Unsupported shader stage in manifest: $shader_stage" >&2
            exit 1
        fi
        for numeric in "$static_combo" "$dynamic_combo" "$vertex_format" "$vertex_inputs" "$constant_bytes" "$sampler_mask" "$fragment_outputs"; do
            if [[ ! "$numeric" =~ ^[0-9]+$ ]]; then
                echo "Non-numeric shader manifest field for $shader_name: $numeric" >&2
                exit 1
            fi
        done
        if [[ "$shader_path" != /app0/* || ! -f "$PACKAGE_DIR/${shader_path#/app0/}" ]]; then
            echo "Missing manifest-referenced shader binary: $shader_path" >&2
            exit 1
        fi
        manifest_key="$shader_name:$shader_stage:$static_combo:$dynamic_combo:$vertex_format"
        if [[ "|$manifest_keys|" == *"|$manifest_key|"* ]]; then
            echo "Duplicate shader manifest key: $manifest_key" >&2
            exit 1
        fi
        manifest_keys="${manifest_keys:+$manifest_keys|}$manifest_key"
        manifest_entries=$((manifest_entries + 1))
    done < "$SHADER_MANIFEST"
    if [[ "$manifest_entries" -eq 0 ]]; then
        echo "Shader manifest contains no entries: $SHADER_MANIFEST" >&2
        exit 1
    fi
fi

pushd "$PACKAGE_DIR" >/dev/null
"$PKGTOOL" sfo_new sce_sys/param.sfo
"$PKGTOOL" sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
"$PKGTOOL" sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value "$VERSION"
"$PKGTOOL" sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0
"$PKGTOOL" sfo_setentry sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value gd
"$PKGTOOL" sfo_setentry sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value "$CONTENT_ID"
"$PKGTOOL" sfo_setentry sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
"$PKGTOOL" sfo_setentry sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0
"$PKGTOOL" sfo_setentry sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value "$TITLE"
"$PKGTOOL" sfo_setentry sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value "$TITLE_ID"
"$PKGTOOL" sfo_setentry sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value "$VERSION"

PACKAGE_FILES="eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png sce_module/libc.prx sce_module/libSceFios2.prx"
if [[ "$VARIANT" == "monolithic" ]]; then
    PACKAGE_FILES="$PACKAGE_FILES kisak_ps4_content_probe.txt kisak_diagnostic.vert.sb kisak_diagnostic.frag.sb kisak_texture_sample.frag.sb kisak_present.frag.sb kisak_depth_clear.frag.sb kisak_reference_cube.vert.sb kisak_reference_cube.frag.sb kisak_scaleform_ordered.vert.sb kisak_scaleform_ordered.frag.sb kisak_diagnostic.manifest"
    for flash in "${SCALEFORM_FLASH_FILES[@]}"; do
        PACKAGE_FILES="$PACKAGE_FILES resource/flash/$flash"
    done
fi
"$CREATE_GP4" -out=pkg.gp4 -content-id="$CONTENT_ID" -files "$PACKAGE_FILES"
# create-gp4 knows the standard PS4 roots but does not emit arbitrary Source
# content directories. Keep the Scaleform tree in the GP4 root explicitly so
# PkgTool can resolve resource/flash while building the package.
if [[ "$VARIANT" == "monolithic" ]] && ! grep -q '<dir targ_name="resource">' pkg.gp4; then
    perl -0pi -e 's#(\s*<dir targ_name="sce_module" />)#$1\n\t\t<dir targ_name="resource"><dir targ_name="flash" /></dir>#' pkg.gp4
fi
"$PKGTOOL" pkg_build pkg.gp4 .
"$PKGTOOL" pkg_validate --verbose "$PACKAGE"
popd >/dev/null

echo "Kisak PS4 bootstrap package: $PACKAGE_DIR/$PACKAGE"
