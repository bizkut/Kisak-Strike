#!/usr/bin/env python3

"""Guarded diagnostics and opt-in development-gate release for Kisak."""

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

from ps4debug import PS4Debug, PS4DebugException, ResponseCode, VMProtection


DEFAULT_HOST = "10.0.1.157"
DEFAULT_PORT = 744
DEFAULT_TITLE_ID = "KISK00002"
DEFAULT_PROTOCOL_VERSION = "1.3"
DEFAULT_OELF = "build-ps4-engine/kisak_ps4_monolithic.oelf"
ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIQQQQQQ")
SECTION_HEADER = struct.Struct("<IIQQQQIIQQ")
SYMBOL_ENTRY = struct.Struct("<IBBHQQ")
ELF_MAGIC = b"\x7fELF"
ELF_CLASS_64 = 2
ELF_DATA_LITTLE_ENDIAN = 1
ELF_MACHINE_X86_64 = 62
PT_LOAD = 1
SHT_STRTAB = 3
SHT_SYMTAB = 2
SHN_UNDEF = 0
STT_OBJECT = 1
PF_EXECUTE = 1
PF_WRITE = 2
PF_READ = 4
HASH_CHUNK_SIZE = 1024 * 1024
MARKER_PATTERN = re.compile(
    rb"kisak-ps4: build marker ([A-Za-z0-9_.:+/-]{1,128})"
)
TITLE_ID_PATTERN = re.compile(r"^[A-Z]{4}[0-9]{5}$")
DEV_GATE_SYMBOL = "g_KisakPs4DevAttachGate"
DEV_GATE_MARKER_SYMBOL = "g_KisakPs4DevAttachGateMarker"
DEV_GATE_MARKER = b"kisak-ps4: dev attach gate v1\0"
DEV_GATE_HOLD = 0x4B4953414B484F4C
DEV_GATE_RELEASE = 0x4B4953414B474F21
DEV_GATE_TIMEOUT = 0x4B4953414B54494D
DEV_GATE_SIZE = 8
STACK_CAPTURE_SIZE = 512
ATTACH_STOP_TIMEOUT = 5.0
REGISTER_NAMES = (
    "rax",
    "rbx",
    "rcx",
    "rdx",
    "rsi",
    "rdi",
    "rbp",
    "rsp",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
    "r15",
    "rip",
    "rflags",
    "trapno",
    "err",
)


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
class ElfSymbol:
    name: str
    virtual_address: int
    size: int
    info: int
    section_index: int

    @property
    def symbol_type(self) -> int:
        return self.info & 0x0F


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


def resolve_elf_symbol(path: pathlib.Path, name: str) -> ElfSymbol:
    """Resolve one exact, defined symbol from an ELF64 static symbol table."""
    target_name = name.encode("ascii")
    file_size = path.stat().st_size
    matches: list[ElfSymbol] = []

    with path.open("rb") as source:
        header_data = source.read(ELF_HEADER.size)
        if len(header_data) != ELF_HEADER.size:
            raise GuardError("OELF is too small to contain an ELF64 header")
        header = ELF_HEADER.unpack(header_data)
        section_header_offset = header[6]
        section_header_size = header[11]
        section_header_count = header[12]

        if section_header_size != SECTION_HEADER.size:
            raise GuardError(
                f"unexpected ELF section-header size: {section_header_size}"
            )
        if not 1 <= section_header_count <= 65535:
            raise GuardError(
                f"invalid ELF section-header count: {section_header_count}"
            )
        section_table_end = (
            section_header_offset + section_header_count * section_header_size
        )
        if section_header_offset < ELF_HEADER.size or section_table_end > file_size:
            raise GuardError("ELF section-header table is outside the OELF")

        source.seek(section_header_offset)
        sections = []
        for _ in range(section_header_count):
            data = source.read(SECTION_HEADER.size)
            if len(data) != SECTION_HEADER.size:
                raise GuardError("truncated ELF section-header table")
            sections.append(SECTION_HEADER.unpack(data))

        for section in sections:
            section_type = section[1]
            if section_type != SHT_SYMTAB:
                continue
            symbol_offset = section[4]
            symbol_size = section[5]
            string_section_index = section[6]
            symbol_entry_size = section[9]
            if symbol_entry_size != SYMBOL_ENTRY.size:
                raise GuardError(
                    f"unexpected ELF symbol-entry size: {symbol_entry_size}"
                )
            if symbol_size % symbol_entry_size != 0:
                raise GuardError("ELF symbol table has a partial entry")
            if symbol_offset + symbol_size > file_size:
                raise GuardError("ELF symbol table is outside the OELF")
            if not 0 <= string_section_index < len(sections):
                raise GuardError("ELF symbol table has an invalid string-table link")

            string_section = sections[string_section_index]
            if string_section[1] != SHT_STRTAB:
                raise GuardError("ELF symbol table does not link to a string table")
            string_offset = string_section[4]
            string_size = string_section[5]
            if string_offset + string_size > file_size:
                raise GuardError("ELF string table is outside the OELF")
            source.seek(string_offset)
            strings = source.read(string_size)
            if len(strings) != string_size:
                raise GuardError("truncated ELF string table")

            source.seek(symbol_offset)
            for _ in range(symbol_size // symbol_entry_size):
                data = source.read(SYMBOL_ENTRY.size)
                if len(data) != SYMBOL_ENTRY.size:
                    raise GuardError("truncated ELF symbol table")
                (
                    name_offset,
                    info,
                    _other,
                    section_index,
                    virtual_address,
                    size,
                ) = SYMBOL_ENTRY.unpack(data)
                if name_offset >= len(strings):
                    raise GuardError("ELF symbol name is outside the string table")
                name_end = strings.find(b"\0", name_offset)
                if name_end < 0:
                    raise GuardError("ELF symbol name is not NUL-terminated")
                if strings[name_offset:name_end] != target_name:
                    continue
                if section_index == SHN_UNDEF:
                    continue
                matches.append(
                    ElfSymbol(name, virtual_address, size, info, section_index)
                )

    if not matches:
        raise GuardError(f"OELF has no defined symbol named {name}")
    if len(matches) != 1:
        raise GuardError(f"OELF has multiple defined symbols named {name}")
    return matches[0]


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


def _segment_protection(segment: LoadSegment) -> int:
    protection = 0
    if segment.flags & PF_READ:
        protection |= int(VMProtection.READ)
    if segment.flags & PF_WRITE:
        protection |= int(VMProtection.WRITE)
    if segment.flags & PF_EXECUTE:
        protection |= int(VMProtection.EXECUTE)
    return protection


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
        for segment in identity.segments:
            if segment.memory_size == 0:
                continue
            start = bias + segment.virtual_address
            end = start + segment.memory_size
            required = _segment_protection(segment)
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
    if (
        refreshed.title_id != target.title_id
        or refreshed.path != target.path
        or refreshed.content_id != target.content_id
    ):
        raise GuardError(
            "process identity changed while maps were being collected; refusing PID"
        )
    return maps


async def wait_for_running_image(
    client: Any,
    title_id: str,
    identity: OelfIdentity,
    *,
    timeout: float,
    interval: float,
) -> tuple[TargetProcess, list[Any], ResolvedImage]:
    """Wait until the exact title is running the verified local OELF image."""
    deadline = time.monotonic() + timeout
    last_error: Exception | None = None
    while True:
        target = await find_title_process(client, title_id)
        if target is not None:
            try:
                maps = await get_guarded_maps(client, target)
                resolved = await resolve_running_image(client, target, maps, identity)
                return target, maps, resolved
            except (GuardError, OSError, PS4DebugException, ValueError) as error:
                last_error = error

        if time.monotonic() >= deadline:
            detail = f": last image guard failed: {last_error}" if last_error else ""
            raise GuardError(
                f"timed out after {timeout:g}s waiting for verified {title_id} image"
                f"{detail}"
            )
        await asyncio.sleep(min(interval, max(0.0, deadline - time.monotonic())))


def _symbol_has_segment_protection(
    identity: OelfIdentity,
    symbol: ElfSymbol,
    required_protection: int,
) -> bool:
    end = symbol.virtual_address + symbol.size
    for segment in identity.segments:
        segment_end = segment.virtual_address + segment.memory_size
        if (
            segment.virtual_address <= symbol.virtual_address
            and end <= segment_end
            and (_segment_protection(segment) & required_protection)
            == required_protection
        ):
            return True
    return False


def validate_dev_gate_symbols(
    identity: OelfIdentity,
    gate: ElfSymbol,
    marker: ElfSymbol,
) -> None:
    if gate.symbol_type != STT_OBJECT or gate.size != DEV_GATE_SIZE:
        raise GuardError(
            f"{DEV_GATE_SYMBOL} must be an {DEV_GATE_SIZE}-byte ELF object"
        )
    if gate.virtual_address % DEV_GATE_SIZE != 0:
        raise GuardError(f"{DEV_GATE_SYMBOL} is not {DEV_GATE_SIZE}-byte aligned")
    if not _symbol_has_segment_protection(
        identity,
        gate,
        int(VMProtection.READ | VMProtection.WRITE),
    ):
        raise GuardError(f"{DEV_GATE_SYMBOL} is not in a readable/writable segment")

    if marker.symbol_type != STT_OBJECT or marker.size != len(DEV_GATE_MARKER):
        raise GuardError(
            f"{DEV_GATE_MARKER_SYMBOL} has an unexpected ELF type or size"
        )
    if not _symbol_has_segment_protection(
        identity,
        marker,
        int(VMProtection.READ),
    ):
        raise GuardError(f"{DEV_GATE_MARKER_SYMBOL} is not in a readable segment")


async def _release_dev_gate_while_stopped(
    client: Any,
    target: TargetProcess,
    identity: OelfIdentity,
    expected_image: ResolvedImage,
    gate: ElfSymbol,
    marker: ElfSymbol,
) -> dict[str, Any]:
    """Revalidate and release a gate while the caller holds the target stopped."""
    validate_dev_gate_symbols(identity, gate, marker)

    maps = await get_guarded_maps(client, target)
    resolved = await resolve_running_image(client, target, maps, identity)
    if resolved != expected_image:
        raise GuardError("runtime image changed between polling and debugger attach")

    gate_address = resolved.load_bias + gate.virtual_address
    marker_address = resolved.load_bias + marker.virtual_address
    if not _range_is_covered(
        maps,
        gate_address,
        gate_address + DEV_GATE_SIZE,
        int(VMProtection.READ | VMProtection.WRITE),
    ):
        raise GuardError("runtime development gate is not readable/writable")
    if not _range_is_covered(
        maps,
        marker_address,
        marker_address + len(DEV_GATE_MARKER),
        int(VMProtection.READ),
    ):
        raise GuardError("runtime development-gate marker is not readable")

    remote_marker = await client.read_memory(
        target.pid,
        marker_address,
        len(DEV_GATE_MARKER),
    )
    if remote_marker != DEV_GATE_MARKER:
        raise GuardError("runtime development-gate marker does not match the OELF")

    current_data = await client.read_memory(
        target.pid,
        gate_address,
        DEV_GATE_SIZE,
    )
    if len(current_data) != DEV_GATE_SIZE:
        raise GuardError("development-gate read returned an unexpected length")
    current = struct.unpack("<Q", current_data)[0]
    if current != DEV_GATE_HOLD:
        if current == DEV_GATE_TIMEOUT:
            state = "timed out"
        elif current == DEV_GATE_RELEASE:
            state = "already released"
        else:
            state = f"unexpected value {current:#018x}"
        raise GuardError(f"development gate is not held: {state}")

    await client.write_memory(
        target.pid,
        gate_address,
        struct.pack("<Q", DEV_GATE_RELEASE),
    )
    readback = await client.read_memory(
        target.pid,
        gate_address,
        DEV_GATE_SIZE,
    )
    if readback != struct.pack("<Q", DEV_GATE_RELEASE):
        raise GuardError("development-gate release write did not verify")

    return {
        "gate_address": gate_address,
        "previous_value": DEV_GATE_HOLD,
        "released_value": DEV_GATE_RELEASE,
    }


async def release_dev_attach_gate(
    client: Any,
    target: TargetProcess,
    identity: OelfIdentity,
    expected_image: ResolvedImage,
    gate: ElfSymbol,
    marker: ElfSymbol,
    *,
    debug_port: int,
) -> dict[str, Any]:
    """Attach stopped, revalidate every guard, release once, then detach/resume."""
    async with client.debugger(
        target.pid,
        port=debug_port,
        resume=False,
        event_read_timeout=None,
    ) as context:
        await _stop_attached_target(context)
        mutation = await _release_dev_gate_while_stopped(
            client,
            target,
            identity,
            expected_image,
            gate,
            marker,
        )
    return {**mutation, "resumed_by": "verified debugger detach"}


async def _stop_attached_target(context: Any) -> Any:
    """Deliver SIGSTOP and wait for its interrupt before guarded memory access."""
    ready = asyncio.Event()
    stopped: dict[str, Any] = {}

    async def on_stop(event: Any) -> None:
        event.resume = False
        if "event" not in stopped:
            stopped["event"] = event
            ready.set()

    context.register_callback(on_stop)
    try:
        status = await context.stop_process()
        if status != ResponseCode.SUCCESS:
            raise GuardError(f"debugger failed to stop attached target: {status}")
        try:
            await asyncio.wait_for(ready.wait(), timeout=ATTACH_STOP_TIMEOUT)
        except TimeoutError as error:
            raise GuardError(
                "attached target did not report its requested stop; "
                "refusing post-attach memory access"
            ) from error
        return stopped["event"]
    finally:
        context.register_callback(None)


async def capture_first_debug_event(
    client: Any,
    target: TargetProcess,
    identity: OelfIdentity,
    expected_image: ResolvedImage,
    gate: ElfSymbol,
    marker: ElfSymbol,
    *,
    debug_port: int,
    capture_timeout: float,
) -> dict[str, Any]:
    """Release the held target while attached and capture its first interrupt."""
    ready = asyncio.Event()
    captured: dict[str, Any] = {}

    async with client.debugger(
        target.pid,
        port=debug_port,
        resume=False,
        event_read_timeout=None,
    ) as context:
        await _stop_attached_target(context)

        async def on_event(event: Any) -> None:
            event.resume = False
            if "event" not in captured:
                captured["event"] = event
                ready.set()

        context.register_callback(on_event)
        mutation = await _release_dev_gate_while_stopped(
            client,
            target,
            identity,
            expected_image,
            gate,
            marker,
        )
        status = await context.resume_process()
        if status != ResponseCode.SUCCESS:
            raise GuardError(f"debugger failed to resume released target: {status}")

        try:
            await asyncio.wait_for(ready.wait(), timeout=capture_timeout)
        except TimeoutError as error:
            raise GuardError(
                f"no debug interrupt arrived within {capture_timeout:g}s"
            ) from error

        event = captured["event"]
        interrupt = event.interrupt
        registers = {
            name: int(getattr(interrupt.regs, name)) for name in REGISTER_NAMES
        }
        stack: dict[str, Any] = {
            "address": registers["rsp"],
            "size": 0,
            "hex": "",
        }
        maps = await get_guarded_maps(client, target)
        for memory_map in maps:
            if (
                memory_map.start <= registers["rsp"] < memory_map.end
                and _map_protection(memory_map) & int(VMProtection.READ)
            ):
                stack_size = min(
                    STACK_CAPTURE_SIZE,
                    memory_map.end - registers["rsp"],
                )
                stack_data = await client.read_memory(
                    target.pid,
                    registers["rsp"],
                    stack_size,
                )
                stack = {
                    "address": registers["rsp"],
                    "size": len(stack_data),
                    "hex": stack_data.hex(),
                }
                break

        result = {
            **mutation,
            "event": {
                "kind": event.kind.name,
                "breakpoint_index": event.index,
                "watchpoint_index": event.watchpoint_index,
                "lwpid": interrupt.lwpid,
                "status": interrupt.status,
                "thread_name": interrupt.name,
                "dr6": interrupt.db_regs.dr6,
                "registers": registers,
            },
            "stack": stack,
        }

    return {**result, "post_capture": "verified debugger detach"}


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
        elif isinstance(item, int) and key in {
            "load_bias",
            "gate_address",
            "previous_value",
            "released_value",
        }:
            print(f"{key}: {item:#x}")
        else:
            print(f"{key}: {item}")


def build_parser() -> argparse.ArgumentParser:
    repository = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description=(
            "Title- and image-aware Kisak diagnostics through ps4debug. "
            "The only mutation is an explicitly enabled, guarded development-gate "
            "release. No RPC, payload, kill, reboot, or injection command exists."
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

    release_parser = subparsers.add_parser(
        "release-dev-gate",
        help="attach stopped and release one verified development gate",
    )
    release_parser.add_argument(
        "--oelf",
        type=pathlib.Path,
        default=repository / DEFAULT_OELF,
    )
    release_parser.add_argument("--expect-oelf-sha256", required=True)
    release_parser.add_argument("--expect-marker", action="append", default=[])
    release_parser.add_argument("--wait-timeout", type=float, default=60.0)
    release_parser.add_argument("--poll-interval", type=float, default=0.1)
    release_parser.add_argument("--debug-port", type=int, default=755)
    release_parser.add_argument(
        "--allow-write",
        action="store_true",
        required=True,
        help="acknowledge the single guarded 8-byte gate release write",
    )

    capture_parser = subparsers.add_parser(
        "capture-crash",
        help="release the verified gate while attached and capture one interrupt",
    )
    capture_parser.add_argument(
        "--oelf",
        type=pathlib.Path,
        default=repository / DEFAULT_OELF,
    )
    capture_parser.add_argument("--expect-oelf-sha256", required=True)
    capture_parser.add_argument("--expect-marker", action="append", default=[])
    capture_parser.add_argument("--wait-timeout", type=float, default=60.0)
    capture_parser.add_argument("--poll-interval", type=float, default=0.1)
    capture_parser.add_argument("--debug-port", type=int, default=755)
    capture_parser.add_argument("--capture-timeout", type=float, default=60.0)
    capture_parser.add_argument(
        "--allow-write",
        action="store_true",
        required=True,
        help="acknowledge the single guarded 8-byte gate release write",
    )
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
    if hasattr(args, "debug_port") and not 1 <= args.debug_port <= 65535:
        parser.error("--debug-port must be between 1 and 65535")
    if hasattr(args, "capture_timeout") and args.capture_timeout <= 0:
        parser.error("--capture-timeout must be positive")


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

    if args.command == "wait-process":
        target = await wait_for_title_process(
            client,
            args.title_id,
            timeout=args.wait_timeout,
            interval=args.poll_interval,
        )
        return {
            "mode": "read-only",
            "protocol_version": version,
            "target": _target_dict(target),
        }

    if args.command in {"maps", "release-dev-gate", "capture-crash"}:
        identity = inspect_oelf(
            args.oelf,
            expected_sha256=args.expect_oelf_sha256,
            expected_markers=args.expect_marker,
        )

        gate = marker = None
        if args.command in {"release-dev-gate", "capture-crash"}:
            gate = resolve_elf_symbol(identity.path, DEV_GATE_SYMBOL)
            marker = resolve_elf_symbol(identity.path, DEV_GATE_MARKER_SYMBOL)
            validate_dev_gate_symbols(identity, gate, marker)

        target, maps, resolved = await wait_for_running_image(
            client,
            args.title_id,
            identity,
            timeout=args.wait_timeout,
            interval=args.poll_interval,
        )

    if args.command == "maps":
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

    if args.command == "release-dev-gate":
        assert gate is not None and marker is not None
        mutation = await release_dev_attach_gate(
            client,
            target,
            identity,
            resolved,
            gate,
            marker,
            debug_port=args.debug_port,
        )
        return {
            "mode": "guarded-write",
            "protocol_version": version,
            "target": _target_dict(target),
            "oelf": {
                "path": str(identity.path),
                "size": identity.size,
                "sha256": identity.sha256,
                "markers": sorted({item.name for item in identity.markers}),
            },
            "load_bias": resolved.load_bias,
            "verified_markers": list(resolved.verified_markers),
            **mutation,
        }

    if args.command == "capture-crash":
        assert gate is not None and marker is not None
        capture = await capture_first_debug_event(
            client,
            target,
            identity,
            resolved,
            gate,
            marker,
            debug_port=args.debug_port,
            capture_timeout=args.capture_timeout,
        )
        return {
            "mode": "guarded-capture",
            "protocol_version": version,
            "target": _target_dict(target),
            "oelf": {
                "path": str(identity.path),
                "size": identity.size,
                "sha256": identity.sha256,
                "markers": sorted({item.name for item in identity.markers}),
            },
            "load_bias": resolved.load_bias,
            "verified_markers": list(resolved.verified_markers),
            **capture,
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
