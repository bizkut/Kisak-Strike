#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 8 ]]; then
    echo "usage: $0 <client-archive> <server-archive> <output-object> <ld> <nm> <objcopy> <ar> <source-root>" >&2
    exit 2
fi

CLIENT_ARCHIVE="$1"
SERVER_ARCHIVE="$2"
OUTPUT_OBJECT="$3"
LD_TOOL="$4"
NM_TOOL="$5"
OBJCOPY_TOOL="$6"
AR_TOOL="$7"
SOURCE_ROOT="$8"

PARTIAL_OBJECT="${OUTPUT_OBJECT}.partial"
FILTERED_SERVER_ARCHIVE="${OUTPUT_OBJECT}.filtered.a"
CLIENT_SYMBOLS="${OUTPUT_OBJECT}.client-symbols"
SERVER_SYMBOLS="${OUTPUT_OBJECT}.server-symbols"
LOCALIZE_SYMBOLS="${OUTPUT_OBJECT}.localize-symbols"
CLIENT_MEMBERS="${OUTPUT_OBJECT}.client-members"
SERVER_MEMBERS="${OUTPUT_OBJECT}.server-members"
SHARED_PROTOBUF_MEMBERS="${OUTPUT_OBJECT}.shared-protobuf-members"

export LC_ALL=C

"$AR_TOOL" t "$CLIENT_ARCHIVE" | sort -u > "$CLIENT_MEMBERS"
"$AR_TOOL" t "$SERVER_ARCHIVE" | sort -u > "$SERVER_MEMBERS"
comm -12 "$CLIENT_MEMBERS" "$SERVER_MEMBERS" \
    | awk '/\.pb\.cc\.obj$/' > "$SHARED_PROTOBUF_MEMBERS"

cp "$SERVER_ARCHIVE" "$FILTERED_SERVER_ARCHIVE"
while IFS= read -r member; do
    source_name="${member%.obj}"
    client_source="$SOURCE_ROOT/game/client/generated_proto/$source_name"
    server_source="$SOURCE_ROOT/game/server/generated_proto/$source_name"
    if ! cmp -s \
        "$client_source" \
        "$server_source"; then
        echo "shared protobuf source differs between client and server: $source_name" >&2
        exit 1
    fi
    "$AR_TOOL" d "$FILTERED_SERVER_ARCHIVE" "$member"
done < "$SHARED_PROTOBUF_MEMBERS"

"$NM_TOOL" --extern-only --defined-only --just-symbol-name "$CLIENT_ARCHIVE" \
    | awk 'NF && substr($0, length($0), 1) != ":"' \
    | sort -u > "$CLIENT_SYMBOLS"
"$NM_TOOL" --extern-only --defined-only --just-symbol-name "$FILTERED_SERVER_ARCHIVE" \
    | awk 'NF && substr($0, length($0), 1) != ":"' \
    | sort -u > "$SERVER_SYMBOLS"
comm -12 "$CLIENT_SYMBOLS" "$SERVER_SYMBOLS" > "$LOCALIZE_SYMBOLS"

"$LD_TOOL" -r -m elf_x86_64 --allow-multiple-definition \
    --whole-archive "$FILTERED_SERVER_ARCHIVE" --no-whole-archive \
    -o "$PARTIAL_OBJECT"
"$OBJCOPY_TOOL" --localize-symbols="$LOCALIZE_SYMBOLS" \
    "$PARTIAL_OBJECT" "$OUTPUT_OBJECT"

printf 'Localized %s overlapping client/server symbols in %s\n' \
    "$(wc -l < "$LOCALIZE_SYMBOLS")" "$OUTPUT_OBJECT"
printf 'Reused %s source-identical shared protobuf archive members\n' \
    "$(wc -l < "$SHARED_PROTOBUF_MEMBERS")"
