#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import pathlib
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

    async def get_processes(self):
        return self.processes

    async def get_process_info(self, pid):
        return self.infos[pid]

    async def get_process_maps(self, pid):
        return self.maps

    async def read_memory(self, pid, address, length):
        return self.memory[(pid, address, length)]


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
        self.assertEqual(set(commands), {"probe", "wait-process", "maps"})


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


if __name__ == "__main__":
    unittest.main()
