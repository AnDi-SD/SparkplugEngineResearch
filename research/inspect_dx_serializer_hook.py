#!/usr/bin/env python3
"""Read-only regression check for the PC spDXSerializerHook mesh preload path."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from inspect_serializer_manager import PC_SHA256, image_slice, read_pe, sha256


BODY_HASHES = {
    "destructor": (0x004AA370, 0x30,
        "00261720BA03A1F5989C10AC559F17244347435D82DEEA3EC398AED04F21F8D1"),
    "registration getter": (0x004AA3A0, 0x06,
        "7D3C3CDC9EB89AED4B813DD465CFA77541A0D57006C3E2D24E5D7374281C4DD4"),
    "deleting destructor": (0x004AA410, 0x1E,
        "B845C17438C7DFF409B2167A828DE1E0040E9F7F6BEF134E511A5227E8CDE9B1"),
    "clone": (0x004AA490, 0x49,
        "9123113246D314B3C2B65C662C8821FC60DC1253D0DF2536F4FA8D220C4ED42F"),
    "ReadDXMeshDataInfo": (0x004AA4E0, 0x38F,
        "BFBB9DF411F4A9532283879CC3BB73B11E327E3C2906EA242EFAE52D1F1CDC97"),
    "batch-and-load": (0x004AA870, 0x30C,
        "E9538C477C5F332A7B80D7C3466535024C1F94BC9B45CA211506D75E4938E4DB"),
    "platform slot wrapper": (0x004AAB80, 0x3C,
        "9FD1CF12A59F7EB0AC5F91279596D117F5009DD0911B966CDCBF565C35CB81BE"),
    "vtable": (0x006EF3C8, 0x20,
        "B8436A5022F15E1308B44366F8132A626C63D69594FE6DFFF10220931C03CB97"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def direct_calls(body: bytes, virtual_address: int) -> list[int]:
    result: list[int] = []
    for offset in range(len(body) - 4):
        if body[offset] != 0xE8:
            continue
        displacement = struct.unpack_from("<i", body, offset + 1)[0]
        result.append((virtual_address + offset + 5 + displacement) & 0xFFFFFFFF)
    return result


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pc", type=Path,
        default=root / "local-data" / "pc-pristine" / "WinxClub.exe")
    args = parser.parse_args()

    pc = args.pc.read_bytes()
    image_base, sections = read_pe(pc)
    failures: list[str] = []
    checks = 0

    def check(label: str, actual: object, expected: object) -> None:
        nonlocal checks
        checks += 1
        ok = actual == expected
        print(f"{'OK' if ok else 'FAIL':4} {label}: {actual!r}")
        if not ok:
            failures.append(f"{label}: expected {expected!r}, got {actual!r}")

    check("PC SHA-256", sha256(pc), PC_SHA256)
    check("PC image base", image_base, 0x00400000)
    check("class name", b"spDXSerializerHook\0" in pc, True)
    check(
        "exact containing source path",
        b"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXMesh.cpp\0" in pc,
        True,
    )

    for label, (address, size, expected_hash) in BODY_HASHES.items():
        body = image_slice(pc, sections, address - image_base, size)
        check(f"{label} body", digest(body), expected_hash)

    initializer = image_slice(pc, sections, 0x006D4D10 - image_base, 0x26)
    check("initializer class ID", struct.pack("<I", 0x0D832A30) in initializer, True)
    check("initializer direct base ID", struct.pack("<I", 0x18092F8D) in initializer, True)
    check("initializer registration", struct.pack("<I", 0x007631B0) in initializer, True)
    check("initializer class-name pointer", struct.pack("<I", 0x006EF6BC) in initializer, True)
    check("initializer factory", struct.pack("<I", 0x004AA430) in initializer, True)
    check(
        "protected factory thunk",
        image_slice(pc, sections, 0x004AA430 - image_base, 6),
        bytes.fromhex("FF 25 98 2D 3B 01"),
    )

    vtable = struct.unpack(
        "<8I", image_slice(pc, sections, 0x006EF3C8 - image_base, 0x20))
    check(
        "PC vtable",
        vtable,
        (0x004AA410, 0x005B7A00, 0x004AA490, 0x0040ECE0,
         0x004AA3A0, 0x00408350, 0x00408370, 0x004AAB80),
    )
    check("PC platform hook slot offset", vtable.index(0x004AAB80) * 4, 0x1C)

    destructor = image_slice(pc, sections, 0x004AA370 - image_base, 0x30)
    check("destructor installs own vtable", struct.pack("<I", 0x006EF3C8) in destructor, True)
    check("destructor addresses list state +0x10", b"\x8d\x7e\x10" in destructor, True)
    check("destructor frees list sentinel +0x14", b"\x8b\x47\x04" in destructor, True)

    info = image_slice(pc, sections, 0x004AA4E0 - image_base, 0x38F)
    info_calls = direct_calls(info, 0x004AA4E0)
    check("mesh-info calls data-block ReadHeader", info_calls.count(0x004728F0), 2)
    check("mesh-info calls data-block SkipData", info_calls.count(0x00472AC0), 2)
    check("mesh-info initializes SBOO bytes", b"\xc6\x44\x24\x14\x53" in info, True)
    check("mesh-info tests field ID 1", b"\x48\x74" in info, True)

    batching = image_slice(pc, sections, 0x004AA870 - image_base, 0x30C)
    calls = direct_calls(batching, 0x004AA870)
    check("batch path scans FAT from first", calls.count(0x00465F00), 2)
    check("batch path advances FAT iterator", calls.count(0x00465F20), 2)
    check("batch path parses mesh info twice", calls.count(0x004AA4E0), 2)
    check("batch path probes resource cache", calls.count(0x004586B0), 1)
    check("batch path filters spMeshData", struct.pack("<I", 0x33C34CF0) in batching, True)
    check("batch path strict vertex limit literal", batching.count(struct.pack("<I", 0x4E20)), 2)
    check("batch path allocates 0x2C-byte shared object", b"\x6a\x2c" in batching, True)
    check("batch path constructs shared mesh", calls.count(0x004A9610), 1)
    check("batch path publishes active shared mesh", struct.pack("<I", 0x00763148) in batching, True)
    check("batch path checks spNamedObject", struct.pack("<I", 0x44DE07FD) in batching, True)
    check("batch path applies FAT name", calls.count(0x004130F0), 1)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}"
    )
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
