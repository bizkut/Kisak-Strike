#!/usr/bin/env python3

import pathlib
import struct
import sys
import zlib


def convert_movie(path: pathlib.Path) -> bool:
    data = path.read_bytes()
    if len(data) < 8:
        raise ValueError(f"{path}: SWF header is truncated")
    if data[:3] == b"FWS":
        return False
    if data[:3] != b"CWS":
        raise ValueError(f"{path}: unsupported SWF signature {data[:3]!r}")

    declared_size = struct.unpack_from("<I", data, 4)[0]
    body = zlib.decompress(data[8:])
    converted = b"FWS" + data[3:8] + body
    if len(converted) != declared_size:
        raise ValueError(
            f"{path}: declared size {declared_size} differs from "
            f"decompressed size {len(converted)}"
        )
    path.write_bytes(converted)
    return True


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <resource/flash directory>", file=sys.stderr)
        return 2

    flash_dir = pathlib.Path(sys.argv[1])
    if not flash_dir.is_dir():
        print(f"missing Scaleform flash directory: {flash_dir}", file=sys.stderr)
        return 2

    converted = 0
    for movie in sorted(flash_dir.glob("*.swf")):
        converted += int(convert_movie(movie))
    print(f"Converted {converted} compressed Scaleform SWF files to FWS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
