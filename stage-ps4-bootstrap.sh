#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PS4_HOST="${PS4_HOST:-10.0.1.157}"
PS4_FTP_PORT="${PS4_FTP_PORT:-2121}"
PS4_PKG_DIR="${PS4_PKG_DIR:-/data/pkg}"
VARIANT="${KISAK_PS4_VARIANT:-bootstrap}"
if [[ "$VARIANT" == "monolithic" ]]; then
    PACKAGE="IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg"
    DEFAULT_PACKAGE="$ROOT_DIR/build-ps4-engine/package/$PACKAGE"
else
    PACKAGE="IV0000-KISK00001_00-KISAKBOOTSTRAP00.pkg"
    DEFAULT_PACKAGE="$ROOT_DIR/build-ps4-bootstrap/package/$PACKAGE"
fi
PACKAGE_PATH="${KISAK_PS4_PACKAGE:-$DEFAULT_PACKAGE}"

if [[ ! -f "$PACKAGE_PATH" ]]; then
    "$ROOT_DIR/package-ps4-bootstrap.sh"
fi

REMOTE_URL="ftp://$PS4_HOST:$PS4_FTP_PORT$PS4_PKG_DIR/$PACKAGE"
curl --fail --silent --show-error --ftp-create-dirs -T "$PACKAGE_PATH" "$REMOTE_URL"
curl --fail --silent --show-error "ftp://$PS4_HOST:$PS4_FTP_PORT$PS4_PKG_DIR/" | grep -F "$PACKAGE"

echo "Staged Kisak PS4 bootstrap package: $REMOTE_URL"
