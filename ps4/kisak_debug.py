#!/usr/bin/env python3

"""Guarded, read-only diagnostics for a Kisak process on ps4debug."""

from __future__ import annotations

import argparse
import asyncio
import dataclasses
import hashlib
import json
import os
import pathlib
import re
import struct
import sys
import time
from collections.abc import Sequence
from typing import Any

from ps4debug import PS4Debug, PS4DebugException, VMProtection


DEFAULT_HOST = "10.0.1.157"
DEFAULT_PORT = 744
DEFAULT_TITLE_ID = "KISK00002"
DEFAULT_PROTOCOL_VERSION = "1.3"
DEFAULT_OELF = "build-ps4-engine/kisak_ps4_monolithic.oelf"
ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIQQQQQQ")
ELF_MAGIC = b"\x7fELF"
ELF_CLASS_64 = 2
ELF_DATA_LITTLE_ENDIAN = 1
ELF_MACHINE_X86_64 = 62
PT_LOAD = 1
PF_EXECUTE = 1
PF_WRITE = 2
PF_READ = 4
HASH_CHUNK_SIZE = 1024 * 1024
MARKER_PATTERN = re.compile(
    rb"kisak-ps4: build marker ([A-Za-z0-9_.:+/-]{1,128})"
)
TITLE_ID_PATTERN = re.compile(r"^[A-Z]{4}[0-9]{5}$")


class GuardError(RuntimeError):
    """Raised when a target identity or artifact guard fails."""


@dataclasses.dataclass(frozen=True)
class LoadSegment:
    flags: int
    offset: int
    virtual_address: int
    file_size: int
    memory_size: int
    alignment: int

    @property
    def executable(self) -> bool:
        return bool(self.flags & PF_EXECUTE)


@dataclasses.dataclass(frozen=True)
class MarkerReference:
    name: str
    virtual_address: int

    @property
    def data(self) -> bytes:
        return f"kisak-ps4: build marker {self.name}".encode("ascii")


@dataclasses.dataclass(frozen=True)
class OelfIdentity:
    path: pathlib.Path
    size: int
    sha256: str
    segments: tuple[LoadSegment, ...]
    markers: tuple[MarkerReference, ...]


@dataclasses.dataclass(frozen=True)
class TargetProcess:
    pid: int
    process_name: str
    path: str
    title_id: str
    content_id: str


@dataclasses.dataclass(frozen=True)
class ResolvedImage:
    load_bias: int
    verified_markers: tuple[str, ...]


def _align_down(value: int, alignment: int) -> int:
    if alignment <= 1:
        return value
    return value - value % alignment


def _sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(HASH_CHUNK_SIZE):
            digest.update(chunk)
    return digest.hexdigest()


def _read_load_segments(path: pathlib.Path) -> tuple[LoadSegment, ...]:
    with path.open("rb") as source:
        header_data = source.read(ELF_HEADER.size)
        if len(header_data) != ELF_HEADER.size:
            raise GuardError("OELF is too small to contain an ELF64 header")

        (
            identification,
            _file_type,
            machine,
            _version,
            _entry,
            program_header_offset,
            _section_header_offset,
            _flags,
            header_size,
            program_header_size,
            program_header_count,
            _section_header_size,
            _section_header_count,
            _section_name_index,
        ) = ELF_HEADER.unpack(header_data)

        if identification[:4] != ELF_MAGIC:
            raise GuardError("artifact is not an ELF/OELF file")
        if identification[4] != ELF_CLASS_64:
            raise GuardError("OELF is not ELF64")
        if identification[5] != ELF_DATA_LITTLE_ENDIAN:
            raise GuardError("OELF is not little-endian")
        if machine != ELF_MACHINE_X86_64:
            raise GuardError(f"OELF machine is not x86-64: {machine}")
        if header_size != ELF_HEADER.size:
            raise GuardError(f"unexpected ELF header size: {header_size}")
        if program_header_size != PROGRAM_HEADER.size:
            raise GuardError(
                f"unexpected ELF program-header size: {program_header_size}"
            )
        if not 1 <= program_header_count <= 256:
            raise GuardError(
                f"invalid ELF program-header count: {program_header_count}"
            )

        source.seek(program_header_offset)
        segments = []
        for _ in range(program_header_count):
            data = source.read(PROGRAM_HEADER.size)
            if len(data) != PROGRAM_HEADER.size:
                raise GuardError("truncated ELF program-header table")
            (
                segment_type,
                flags,
                offset,
                virtual_address,
                _physical_address,
                file_size,
                memory_size,
                alignment,
            ) = PROGRAM_HEADER.unpack(data)
            if segment_type == PT_LOAD:
                if file_size > memory_size:
                    raise GuardError("ELF load segment has filesz larger than memsz")
                segments.append(
                    LoadSegment(
                        flags,
                        offset,
                        virtual_address,
                        file_size,
                        memory_size,
                        alignment,
                    )
                )

    if not segments:
        raise GuardError("OELF has no loadable segments")
    if not any(segment.executable for segment in segments):
        raise GuardError("OELF has no executable load segment")
    return tuple(segments)


def _find_loaded_markers(
    path: pathlib.Path,
    segments: tuple[LoadSegment, ...],
) -> tuple[MarkerReference, ...]:
    matches: set[tuple[str, int]] = set()
    overlap = b""
    file_offset = 0

    with path.open("rb") as source:
        while chunk := source.read(HASH_CHUNK_SIZE):
            data = overlap + chunk
            data_offset = file_offset - len(overlap)
            for match in MARKER_PATTERN.finditer(data):
                marker_offset = data_offset + match.start()
                for segment in segments:
                    if segment.offset <= marker_offset < (
                        segment.offset + segment.file_size
                    ):
                        virtual_address = (
                            segment.virtual_address
                            + marker_offset
                            - segment.offset
                        )
                        name = match.group(1).decode("ascii")
                        matches.add((name, virtual_address))
                        break
            file_offset += len(chunk)
            overlap = data[-256:]

    if not matches:
        raise GuardError("OELF contains no loaded Kisak PS4 build marker")
    return tuple(
        MarkerReference(name, address)
        for name, address in sorted(matches)
    )


def inspect_oelf(
    path: pathlib.Path,
    *,
    expected_sha256: str | None = None,
    expected_markers: Sequence[str] = (),
) -> OelfIdentity:
    path = path.expanduser().resolve()
    if not path.is_file():
        raise GuardError(f"OELF is not a regular file: {path}")

    sha256 = _sha256_file(path)
    if expected_sha256 is not None:
        expected_sha256 = expected_sha256.lower()
        if not re.fullmatch(r"[0-9a-f]{64}", expected_sha256):
            raise GuardError("expected OELF SHA-256 must be 64 hexadecimal digits")
        if sha256 != expected_sha256:
            raise GuardError(
                f"OELF SHA-256 mismatch: expected {expected_sha256}, found {sha256}"
            )

    segments = _read_load_segments(path)
    markers = _find_loaded_markers(path, segments)
    marker_names = {marker.name for marker in markers}
    for expected_marker in expected_markers:
        if expected_marker not in marker_names:
            raise GuardError(
                f"OELF is missing expected build marker: {expected_marker}"
            )

    return OelfIdentity(path, path.stat().st_size, sha256, segments, markers)


async def require_protocol(client: Any, expected_version: str) -> str:
    version = await client.get_version()
    if version != expected_version:
        raise GuardError(
            f"ps4debug protocol mismatch: expected {expected_version}, found {version}"
        )
    return version


async def find_title_process(
    client: Any,
    title_id: str,
) -> TargetProcess | None:
    matches = []
    for process in await client.get_processes():
        try:
            info = await client.get_process_info(process.pid)
        except (OSError, PS4DebugException, ValueError):
            continue
        if info.title_id == title_id:
            matches.append(
                TargetProcess(
                    pid=process.pid,
                    process_name=process.name,
                    path=info.path,
                    title_id=info.title_id,
                    content_id=info.content_id,
                )
            )

    if len(matches) > 1:
        pids = ", ".join(str(match.pid) for match in matches)
        raise GuardError(f"multiple {title_id} processes are running: {pids}")
    return matches[0] if matches else None


async def wait_for_title_process(
    client: Any,
    title_id: str,
    *,
    timeout: float,
    interval: float,
) -> TargetProcess:
    deadline = time.monotonic() + timeout
    while True:
        target = await find_title_process(client, title_id)
        if target is not None:
            return target
        if time.monotonic() >= deadline:
            raise GuardError(
                f"timed out after {timeout:g}s waiting for title {title_id}"
            )
        await asyncio.sleep(min(interval, max(0.0, deadline - time.monotonic())))


def _map_protection(memory_map: Any) -> int:
    return int(memory_map.prot)


def _range_is_covered(
    maps: Sequence[Any],
    start: int,
    end: int,
    required_protection: int,
) -> bool:
    cursor = start
    for memory_map in sorted(maps, key=lambda item: item.start):
        if memory_map.end <= cursor:
            continue
        if memory_map.start > cursor:
            return False
        if (_map_protection(memory_map) & required_protection) != required_protection:
            return False
        cursor = min(end, memory_map.end)
        if cursor >= end:
            return True
    return False


def candidate_load_biases(
    maps: Sequence[Any],
    identity: OelfIdentity,
) -> tuple[int, ...]:
    executable_segments = [
        segment for segment in identity.segments if segment.executable
    ]
    anchor = executable_segments[0]
    alignment = max(anchor.alignment, 1)
    anchor_page = _align_down(anchor.virtual_address, alignment)
    candidates = set()

    for memory_map in maps:
        if not (_map_protection(memory_map) & int(VMProtection.EXECUTE)):
            continue
        bias = memory_map.start - anchor_page
        if bias < 0:
            continue

        valid = True
        for segment in executable_segments:
            start = bias + segment.virtual_address
            end = start + segment.memory_size
            required = int(VMProtection.READ | VMProtection.EXECUTE)
            if not _range_is_covered(maps, start, end, required):
                valid = False
                break
        if valid:
            candidates.add(bias)

    return tuple(sorted(candidates))


async def resolve_running_image(
    client: Any,
    target: TargetProcess,
    maps: Sequence[Any],
    identity: OelfIdentity,
) -> ResolvedImage:
    candidates = candidate_load_biases(maps, identity)
    verified: list[ResolvedImage] = []

    for bias in candidates:
        marker_names = []
        for marker in identity.markers:
            address = bias + marker.virtual_address
            if not _range_is_covered(
                maps,
                address,
                address + len(marker.data),
                int(VMProtection.READ),
            ):
                break
            data = await client.read_memory(target.pid, address, len(marker.data))
            if data != marker.data:
                break
            marker_names.append(marker.name)
        else:
            verified.append(ResolvedImage(bias, tuple(marker_names)))

    if not candidates:
        raise GuardError("no runtime map layout matches the local OELF")
    if not verified:
        raise GuardError(
            "runtime executable maps matched, but the Kisak build marker did not"
        )
    if len(verified) != 1:
        biases = ", ".join(f"{item.load_bias:#x}" for item in verified)
        raise GuardError(f"multiple runtime image candidates passed guards: {biases}")
    return verified[0]


async def get_guarded_maps(
    client: Any,
    target: TargetProcess,
) -> list[Any]:
    maps = await client.get_process_maps(target.pid)
    if not maps:
        raise GuardError(f"title {target.title_id} has no reported process maps")
    for memory_map in maps:
        if not 0 <= memory_map.start < memory_map.end <= 0xFFFFFFFFFFFFFFFF:
            raise GuardError(f"invalid process map range: {memory_map!r}")

    refreshed = await client.get_process_info(target.pid)
    if refreshed.title_id != target.title_id:
        raise GuardError(
            "process identity changed while maps were being collected; refusing PID"
        )
    return maps


def _target_dict(target: TargetProcess) -> dict[str, Any]:
    return dataclasses.asdict(target)


def _map_dict(memory_map: Any) -> dict[str, Any]:
    return {
        "name": memory_map.name,
        "start": memory_map.start,
        "end": memory_map.end,
        "offset": memory_map.offset,
        "protection": _map_protection(memory_map),
    }


def _emit(value: dict[str, Any], *, as_json: bool) -> None:
    if as_json:
        print(json.dumps(value, indent=2, sort_keys=True))
        return

    for key, item in value.items():
        if isinstance(item, (dict, list)):
            print(f"{key}: {json.dumps(item, sort_keys=True)}")
        elif isinstance(item, int) and key in {"load_bias"}:
            print(f"{key}: {item:#x}")
        else:
            print(f"{key}: {item}")


def build_parser() -> argparse.ArgumentParser:
    repository = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description=(
            "Read-only, title-aware Kisak diagnostics through ps4debug. "
            "No write, RPC, payload, kill, reboot, or injection operations exist."
        )
    )
    parser.add_argument(
        "--host", default=os.environ.get("KISAK_PS4_HOST", DEFAULT_HOST)
    )
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--network-timeout", type=float, default=15.0)
    parser.add_argument("--title-id", default=DEFAULT_TITLE_ID)
    parser.add_argument("--protocol-version", default=DEFAULT_PROTOCOL_VERSION)
    parser.add_argument("--json", action="store_true")

    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("probe", help="verify the ps4debug protocol version")

    wait_parser = subparsers.add_parser(
        "wait-process",
        help="wait for a freshly selected process with the exact title ID",
    )
    wait_parser.add_argument("--wait-timeout", type=float, default=60.0)
    wait_parser.add_argument("--poll-interval", type=float, default=0.25)

    maps_parser = subparsers.add_parser(
        "maps",
        help="verify process, OELF, load bias, build marker, and list maps",
    )
    maps_parser.add_argument(
        "--oelf",
        type=pathlib.Path,
        default=repository / DEFAULT_OELF,
    )
    maps_parser.add_argument("--expect-oelf-sha256")
    maps_parser.add_argument("--expect-marker", action="append", default=[])
    maps_parser.add_argument("--wait-timeout", type=float, default=10.0)
    maps_parser.add_argument("--poll-interval", type=float, default=0.25)
    return parser


def _validate_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if args.network_timeout <= 0:
        parser.error("--network-timeout must be positive")
    if not TITLE_ID_PATTERN.fullmatch(args.title_id):
        parser.error("--title-id must match AAAA00000")
    if hasattr(args, "wait_timeout") and args.wait_timeout <= 0:
        parser.error("--wait-timeout must be positive")
    if hasattr(args, "poll_interval") and args.poll_interval <= 0:
        parser.error("--poll-interval must be positive")


async def run(args: argparse.Namespace) -> dict[str, Any]:
    client = PS4Debug(
        args.host,
        port=args.port,
        timeout=args.network_timeout,
    )
    version = await require_protocol(client, args.protocol_version)
    if args.command == "probe":
        return {
            "mode": "read-only",
            "host": args.host,
            "port": args.port,
            "protocol_version": version,
        }

    target = await wait_for_title_process(
        client,
        args.title_id,
        timeout=args.wait_timeout,
        interval=args.poll_interval,
    )
    if args.command == "wait-process":
        return {
            "mode": "read-only",
            "protocol_version": version,
            "target": _target_dict(target),
        }

    if args.command == "maps":
        identity = inspect_oelf(
            args.oelf,
            expected_sha256=args.expect_oelf_sha256,
            expected_markers=args.expect_marker,
        )
        maps = await get_guarded_maps(client, target)
        resolved = await resolve_running_image(client, target, maps, identity)
        return {
            "mode": "read-only",
            "protocol_version": version,
            "target": _target_dict(target),
            "oelf": {
                "path": str(identity.path),
                "size": identity.size,
                "sha256": identity.sha256,
                "markers": sorted({marker.name for marker in identity.markers}),
            },
            "load_bias": resolved.load_bias,
            "verified_markers": list(resolved.verified_markers),
            "maps": [_map_dict(memory_map) for memory_map in maps],
        }

    raise GuardError(f"unsupported command: {args.command}")


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    _validate_args(parser, args)
    try:
        result = asyncio.run(run(args))
        _emit(result, as_json=args.json)
        return 0
    except (GuardError, OSError, PS4DebugException, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
