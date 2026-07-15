#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import pathlib
import struct
import tempfile
import types
import unittest

from ps4debug import VMProtection

import kisak_debug as debug


class FakeClient:
    def __init__(self) -> None:
        self.processes = []
        self.infos = {}
        self.maps = []
        self.memory = {}
        self.writes = []
        self.debugger_calls = []

    async def get_processes(self):
        return self.processes

    async def get_process_info(self, pid):
        return self.infos[pid]

    async def get_process_maps(self, pid):
        return self.maps

    async def read_memory(self, pid, address, length):
        return self.memory[(pid, address, length)]

    async def write_memory(self, pid, address, value):
        self.writes.append((pid, address, value))
        self.memory[(pid, address, len(value))] = value

    def debugger(self, pid, **kwargs):
        self.debugger_calls.append((pid, kwargs))
        return FakeDebuggerContext()


class FakeDebuggerContext:
    async def __aenter__(self):
        return self

    async def __aexit__(self, exception_type, exception, traceback):
        return False


def make_oelf(directory: pathlib.Path, marker: str = "unit_test_marker"):
    path = directory / "kisak.oelf"
    identification = bytearray(16)
    identification[:4] = debug.ELF_MAGIC
    identification[4] = debug.ELF_CLASS_64
    identification[5] = debug.ELF_DATA_LITTLE_ENDIAN
    identification[6] = 1
    header = debug.ELF_HEADER.pack(
        bytes(identification),
        3,
        debug.ELF_MACHINE_X86_64,
        1,
        0x400000,
        debug.ELF_HEADER.size,
        0,
        0,
        debug.ELF_HEADER.size,
        debug.PROGRAM_HEADER.size,
        1,
        0,
        0,
        0,
    )
    program_header = debug.PROGRAM_HEADER.pack(
        debug.PT_LOAD,
        debug.PF_READ | debug.PF_EXECUTE,
        0,
        0x400000,
        0,
        0x600,
        0x600,
        0x1000,
    )
    data = bytearray(0x600)
    data[: len(header)] = header
    data[debug.ELF_HEADER.size : debug.ELF_HEADER.size + len(program_header)] = (
        program_header
    )
    marker_data = f"kisak-ps4: build marker {marker}".encode("ascii")
    data[0x200 : 0x200 + len(marker_data)] = marker_data
    path.write_bytes(data)
    return path, marker_data


def make_gate_oelf(directory: pathlib.Path):
    path = directory / "kisak-gate.oelf"
    identification = bytearray(16)
    identification[:4] = debug.ELF_MAGIC
    identification[4] = debug.ELF_CLASS_64
    identification[5] = debug.ELF_DATA_LITTLE_ENDIAN
    identification[6] = 1

    section_offset = 0x1300
    header = debug.ELF_HEADER.pack(
        bytes(identification),
        3,
        debug.ELF_MACHINE_X86_64,
        1,
        0x400000,
        debug.ELF_HEADER.size,
        section_offset,
        0,
        debug.ELF_HEADER.size,
        debug.PROGRAM_HEADER.size,
        2,
        debug.SECTION_HEADER.size,
        5,
        0,
    )
    executable_header = debug.PROGRAM_HEADER.pack(
        debug.PT_LOAD,
        debug.PF_READ | debug.PF_EXECUTE,
        0,
        0x400000,
        0,
        0x600,
        0x600,
        0x1000,
    )
    writable_header = debug.PROGRAM_HEADER.pack(
        debug.PT_LOAD,
        debug.PF_READ | debug.PF_WRITE,
        0x1000,
        0x500000,
        0,
        0x100,
        0x100,
        0x1000,
    )

    data = bytearray(0x1500)
    data[: len(header)] = header
    program_headers = executable_header + writable_header
    data[debug.ELF_HEADER.size : debug.ELF_HEADER.size + len(program_headers)] = (
        program_headers
    )
    build_marker = b"kisak-ps4: build marker unit_test_marker"
    data[0x200 : 0x200 + len(build_marker)] = build_marker
    data[0x300 : 0x300 + len(debug.DEV_GATE_MARKER)] = debug.DEV_GATE_MARKER
    data[0x1000 : 0x1000 + debug.DEV_GATE_SIZE] = struct.pack(
        "<Q", debug.DEV_GATE_HOLD
    )

    strings = (
        b"\0"
        + debug.DEV_GATE_SYMBOL.encode("ascii")
        + b"\0"
        + debug.DEV_GATE_MARKER_SYMBOL.encode("ascii")
        + b"\0"
    )
    gate_name_offset = 1
    marker_name_offset = gate_name_offset + len(debug.DEV_GATE_SYMBOL) + 1
    data[0x1100 : 0x1100 + len(strings)] = strings
    symbols = b"".join(
        (
            debug.SYMBOL_ENTRY.pack(0, 0, 0, 0, 0, 0),
            debug.SYMBOL_ENTRY.pack(
                gate_name_offset,
                (1 << 4) | debug.STT_OBJECT,
                0,
                2,
                0x500000,
                debug.DEV_GATE_SIZE,
            ),
            debug.SYMBOL_ENTRY.pack(
                marker_name_offset,
                (1 << 4) | debug.STT_OBJECT,
                0,
                1,
                0x400300,
                len(debug.DEV_GATE_MARKER),
            ),
        )
    )
    data[0x1200 : 0x1200 + len(symbols)] = symbols

    sections = b"".join(
        (
            debug.SECTION_HEADER.pack(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            debug.SECTION_HEADER.pack(
                0, 1, 6, 0x400000, 0, 0x600, 0, 0, 16, 0
            ),
            debug.SECTION_HEADER.pack(
                0, 1, 3, 0x500000, 0x1000, 0x100, 0, 0, 8, 0
            ),
            debug.SECTION_HEADER.pack(
                0, debug.SHT_STRTAB, 0, 0, 0x1100, len(strings), 0, 0, 1, 0
            ),
            debug.SECTION_HEADER.pack(
                0,
                debug.SHT_SYMTAB,
                0,
                0,
                0x1200,
                len(symbols),
                3,
                1,
                8,
                debug.SYMBOL_ENTRY.size,
            ),
        )
    )
    data[section_offset : section_offset + len(sections)] = sections
    path.write_bytes(data)
    return path, build_marker


class OelfTests(unittest.TestCase):
    def test_inspect_oelf_hashes_and_resolves_loaded_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path, _ = make_oelf(pathlib.Path(temporary))
            expected_hash = hashlib.sha256(path.read_bytes()).hexdigest()

            identity = debug.inspect_oelf(
                path,
                expected_sha256=expected_hash,
                expected_markers=["unit_test_marker"],
            )

            self.assertEqual(identity.sha256, expected_hash)
            self.assertEqual(identity.markers[0].name, "unit_test_marker")
            self.assertEqual(identity.markers[0].virtual_address, 0x400200)

    def test_inspect_oelf_rejects_hash_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path, _ = make_oelf(pathlib.Path(temporary))
            with self.assertRaisesRegex(debug.GuardError, "SHA-256 mismatch"):
                debug.inspect_oelf(path, expected_sha256="0" * 64)

    def test_parser_exposes_only_read_only_commands(self) -> None:
        parser = debug.build_parser()
        commands = next(
            action.choices
            for action in parser._actions
            if getattr(action, "choices", None)
        )
        self.assertEqual(
            set(commands),
            {"probe", "wait-process", "maps", "release-dev-gate"},
        )
        release = commands["release-dev-gate"]
        required = {
            action.dest
            for action in release._actions
            if getattr(action, "required", False)
        }
        self.assertIn("expect_oelf_sha256", required)
        self.assertIn("allow_write", required)

    def test_resolve_gate_symbols_and_validate_segments(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path, _ = make_gate_oelf(pathlib.Path(temporary))
            identity = debug.inspect_oelf(path)
            gate = debug.resolve_elf_symbol(path, debug.DEV_GATE_SYMBOL)
            marker = debug.resolve_elf_symbol(path, debug.DEV_GATE_MARKER_SYMBOL)

        debug.validate_dev_gate_symbols(identity, gate, marker)
        self.assertEqual(gate.virtual_address, 0x500000)
        self.assertEqual(gate.size, debug.DEV_GATE_SIZE)
        self.assertEqual(marker.virtual_address, 0x400300)


class TargetTests(unittest.IsolatedAsyncioTestCase):
    async def test_find_title_process_uses_exact_title_id(self) -> None:
        client = FakeClient()
        client.processes = [
            types.SimpleNamespace(pid=10, name="wrong"),
            types.SimpleNamespace(pid=20, name="eboot.bin"),
        ]
        client.infos = {
            10: types.SimpleNamespace(
                path="/app0/eboot.bin",
                title_id="OTHER0001",
                content_id="wrong",
            ),
            20: types.SimpleNamespace(
                path="/app0/eboot.bin",
                title_id=debug.DEFAULT_TITLE_ID,
                content_id="IV0000-KISK00002_00-KISAKMONOLITHIC0",
            ),
        }

        target = await debug.find_title_process(client, debug.DEFAULT_TITLE_ID)

        self.assertEqual(target.pid, 20)
        self.assertEqual(target.title_id, debug.DEFAULT_TITLE_ID)

    async def test_find_title_process_rejects_ambiguous_pids(self) -> None:
        client = FakeClient()
        client.processes = [
            types.SimpleNamespace(pid=10, name="one"),
            types.SimpleNamespace(pid=20, name="two"),
        ]
        info = types.SimpleNamespace(
            path="/app0/eboot.bin",
            title_id=debug.DEFAULT_TITLE_ID,
            content_id="content",
        )
        client.infos = {10: info, 20: info}

        with self.assertRaisesRegex(debug.GuardError, "multiple KISK00002"):
            await debug.find_title_process(client, debug.DEFAULT_TITLE_ID)

    async def test_resolve_running_image_verifies_remote_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path, marker_data = make_oelf(pathlib.Path(temporary))
            identity = debug.inspect_oelf(path)

        runtime_start = 0x10000000
        load_bias = runtime_start - 0x400000
        memory_map = types.SimpleNamespace(
            name="eboot.bin",
            start=runtime_start,
            end=runtime_start + 0x1000,
            offset=0,
            prot=VMProtection.READ | VMProtection.EXECUTE,
        )
        target = debug.TargetProcess(
            20,
            "eboot.bin",
            "/app0/eboot.bin",
            debug.DEFAULT_TITLE_ID,
            "content",
        )
        marker_address = load_bias + identity.markers[0].virtual_address
        client = FakeClient()
        client.memory[(20, marker_address, len(marker_data))] = marker_data

        resolved = await debug.resolve_running_image(
            client,
            target,
            [memory_map],
            identity,
        )

        self.assertEqual(resolved.load_bias, load_bias)
        self.assertEqual(resolved.verified_markers, ("unit_test_marker",))

    async def test_resolve_running_image_rejects_marker_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path, marker_data = make_oelf(pathlib.Path(temporary))
            identity = debug.inspect_oelf(path)

        runtime_start = 0x10000000
        load_bias = runtime_start - 0x400000
        memory_map = types.SimpleNamespace(
            name="eboot.bin",
            start=runtime_start,
            end=runtime_start + 0x1000,
            offset=0,
            prot=VMProtection.READ | VMProtection.EXECUTE,
        )
        target = debug.TargetProcess(
            20,
            "eboot.bin",
            "/app0/eboot.bin",
            debug.DEFAULT_TITLE_ID,
            "content",
        )
        marker_address = load_bias + identity.markers[0].virtual_address
        client = FakeClient()
        client.memory[(20, marker_address, len(marker_data))] = b"X" * len(
            marker_data
        )

        with self.assertRaisesRegex(debug.GuardError, "build marker did not"):
            await debug.resolve_running_image(
                client,
                target,
                [memory_map],
                identity,
            )

    async def test_release_dev_gate_revalidates_and_writes_once(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path, build_marker = make_gate_oelf(pathlib.Path(temporary))
            identity = debug.inspect_oelf(path)
            gate = debug.resolve_elf_symbol(path, debug.DEV_GATE_SYMBOL)
            marker = debug.resolve_elf_symbol(path, debug.DEV_GATE_MARKER_SYMBOL)

        target = debug.TargetProcess(
            20,
            "eboot.bin",
            "/app0/eboot.bin",
            debug.DEFAULT_TITLE_ID,
            "content",
        )
        load_bias = 0x10000000 - 0x400000
        maps = [
            types.SimpleNamespace(
                name="eboot.bin",
                start=load_bias + 0x400000,
                end=load_bias + 0x400600,
                offset=0,
                prot=VMProtection.READ | VMProtection.EXECUTE,
            ),
            types.SimpleNamespace(
                name="eboot.bin",
                start=load_bias + 0x500000,
                end=load_bias + 0x500100,
                offset=0x1000,
                prot=VMProtection.READ | VMProtection.WRITE,
            ),
        ]
        client = FakeClient()
        client.maps = maps
        client.infos[20] = types.SimpleNamespace(
            path=target.path,
            title_id=target.title_id,
            content_id=target.content_id,
        )
        client.memory[
            (20, load_bias + identity.markers[0].virtual_address, len(build_marker))
        ] = build_marker
        client.memory[
            (20, load_bias + marker.virtual_address, len(debug.DEV_GATE_MARKER))
        ] = debug.DEV_GATE_MARKER
        client.memory[(20, load_bias + gate.virtual_address, debug.DEV_GATE_SIZE)] = (
            struct.pack("<Q", debug.DEV_GATE_HOLD)
        )
        expected = debug.ResolvedImage(load_bias, ("unit_test_marker",))

        result = await debug.release_dev_attach_gate(
            client,
            target,
            identity,
            expected,
            gate,
            marker,
            debug_port=755,
        )

        gate_address = load_bias + gate.virtual_address
        self.assertEqual(result["gate_address"], gate_address)
        self.assertEqual(
            client.writes,
            [(20, gate_address, struct.pack("<Q", debug.DEV_GATE_RELEASE))],
        )
        self.assertEqual(client.debugger_calls[0][0], 20)
        self.assertFalse(client.debugger_calls[0][1]["resume"])

    async def test_release_dev_gate_rejects_timed_out_gate_without_write(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path, build_marker = make_gate_oelf(pathlib.Path(temporary))
            identity = debug.inspect_oelf(path)
            gate = debug.resolve_elf_symbol(path, debug.DEV_GATE_SYMBOL)
            marker = debug.resolve_elf_symbol(path, debug.DEV_GATE_MARKER_SYMBOL)

        target = debug.TargetProcess(
            20,
            "eboot.bin",
            "/app0/eboot.bin",
            debug.DEFAULT_TITLE_ID,
            "content",
        )
        load_bias = 0x10000000 - 0x400000
        client = FakeClient()
        client.maps = [
            types.SimpleNamespace(
                name="eboot.bin",
                start=load_bias + 0x400000,
                end=load_bias + 0x400600,
                offset=0,
                prot=VMProtection.READ | VMProtection.EXECUTE,
            ),
            types.SimpleNamespace(
                name="eboot.bin",
                start=load_bias + 0x500000,
                end=load_bias + 0x500100,
                offset=0x1000,
                prot=VMProtection.READ | VMProtection.WRITE,
            ),
        ]
        client.infos[20] = types.SimpleNamespace(
            path=target.path,
            title_id=target.title_id,
            content_id=target.content_id,
        )
        client.memory[
            (20, load_bias + identity.markers[0].virtual_address, len(build_marker))
        ] = build_marker
        client.memory[
            (20, load_bias + marker.virtual_address, len(debug.DEV_GATE_MARKER))
        ] = debug.DEV_GATE_MARKER
        client.memory[(20, load_bias + gate.virtual_address, debug.DEV_GATE_SIZE)] = (
            struct.pack("<Q", debug.DEV_GATE_TIMEOUT)
        )

        with self.assertRaisesRegex(debug.GuardError, "timed out"):
            await debug.release_dev_attach_gate(
                client,
                target,
                identity,
                debug.ResolvedImage(load_bias, ("unit_test_marker",)),
                gate,
                marker,
                debug_port=755,
            )

        self.assertEqual(client.writes, [])


if __name__ == "__main__":
    unittest.main()
