#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VARIANT="${KISAK_PS4_VARIANT:-bootstrap}"
if [[ "$VARIANT" == "monolithic" ]]; then
    BUILD_DIR="${KISAK_PS4_ENGINE_BUILD_DIR:-$ROOT_DIR/build-ps4-engine}"
    TITLE="Kisak-Strike PS4 Monolithic"
    VERSION="1.28"
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
PACKAGE="$CONTENT_ID.pkg"

if [[ -z "${OO_PS4_TOOLCHAIN:-}" ]]; then
    SETUP="$ROOT_DIR/../tools/setup_openorbis_llvm18_macos.sh"
    if [[ ! -x "$SETUP" ]]; then
        echo "Set OO_PS4_TOOLCHAIN to the OpenOrbis SDK root." >&2
        exit 1
    fi
    OO_PS4_TOOLCHAIN="$($SETUP)"
    export OO_PS4_TOOLCHAIN
fi

if [[ "$VARIANT" == "monolithic" ]]; then
    "$ROOT_DIR/build-ps4-monolithic.sh"
else
    "$ROOT_DIR/build-ps4-bootstrap.sh"
fi

CREATE_GP4="${CREATE_GP4:-$OO_PS4_TOOLCHAIN/bin/macos/create-gp4}"
PKGTOOL="${PKGTOOL:-$OO_PS4_TOOLCHAIN/bin/macos/PkgTool.Core}"
RUNTIME_MODULE_DIR="${RUNTIME_MODULE_DIR:-$OO_PS4_TOOLCHAIN/src/modules}"
ASSET_DIR="${KISAK_PS4_ASSET_DIR:-$ROOT_DIR/../freegnm-examples/videoout-linear/sce_sys}"

for required in "$CREATE_GP4" "$PKGTOOL" "$RUNTIME_MODULE_DIR/libc.prx" \
    "$RUNTIME_MODULE_DIR/libSceFios2.prx" "$ASSET_DIR/icon0.png" \
    "$ASSET_DIR/about/right.sprx" "$EBOOT_INPUT"; do
    if [[ ! -e "$required" ]]; then
        echo "Missing PS4 package input: $required" >&2
        exit 1
    fi
done

rm -rf "$PACKAGE_DIR"
mkdir -p "$DOTNET_BUNDLE_EXTRACT_BASE_DIR" "$PACKAGE_DIR/sce_module" "$PACKAGE_DIR/sce_sys/about"
cp "$EBOOT_INPUT" "$PACKAGE_DIR/eboot.bin"
cp "$RUNTIME_MODULE_DIR/libc.prx" "$PACKAGE_DIR/sce_module/libc.prx"
cp "$RUNTIME_MODULE_DIR/libSceFios2.prx" "$PACKAGE_DIR/sce_module/libSceFios2.prx"
cp "$ASSET_DIR/icon0.png" "$PACKAGE_DIR/sce_sys/icon0.png"
cp "$ASSET_DIR/about/right.sprx" "$PACKAGE_DIR/sce_sys/about/right.sprx"

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
"$CREATE_GP4" -out=pkg.gp4 -content-id="$CONTENT_ID" -files "$PACKAGE_FILES"
"$PKGTOOL" pkg_build pkg.gp4 .
"$PKGTOOL" pkg_validate --verbose "$PACKAGE"
popd >/dev/null

echo "Kisak PS4 bootstrap package: $PACKAGE_DIR/$PACKAGE"
