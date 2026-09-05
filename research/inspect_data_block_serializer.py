#!/usr/bin/env python3
"""Read-only regression check for native spDataBlockSerializer evidence."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from inspect_serializer_manager import (
    PC_SHA256,
    PS2_SHA256,
    image_slice,
    read_elf,
    read_pe,
    sha256,
)


PC_BODIES = {
    "size-code selector": (0x00472730, 0x52,
        "A45AD3DE1B5A1E9EB314AEBC267C60CE558ED3A5B02FB83F92E31C20B181ABBE"),
    "ReadHeader": (0x004728F0, 0x1A9,
        "0B41EA953CEA9708CDDE59E069D5489DAE9F881F6F39C57BB983D678D0BCBB6D"),
    "SkipData": (0x00472AC0, 0x3D,
        "13B896564EE6D29082B1760E83F3A919148E7F1680A45B37163E48FF9096607F"),
    "terminator writer": (0x00472B00, 0x40,
        "7A76B31E0C63FA842BF72F8EBE8EE675F79A57A5675669F4FA61E89005C4EBDF"),
    "direct-field writer": (0x00472B40, 0x6C,
        "5B241BF7305A696228A6B076306D302F8211FC77D54250FC1E11FBAD21A1976D"),
    "constructor": (0x00473000, 0x2D,
        "D5D1E92E35D2259EDDB145A4341570924C4FDAF4167517479F0FC63F500B3A00"),
}

PS2_BODIES = {
    "size-code selector": (0x0017E740, 0x80,
        "2C34A8B54A604A74D8F013F4490AA59DA585A8D03FA24236E47C9A729739C0FE"),
    "terminator writer": (0x0017E7C0, 0x58,
        "AEB49AC079B1F579A2552D4BD410D7EF224EE28B117B6C2D53FCCD99A0B9FDAA"),
    "SkipData": (0x0017E830, 0x58,
        "515E33F6DD51EE4FFF1DFF8C016CA346864A18A2469ECD6EA020B7EE44B0E091"),
    "ReadHeader": (0x0017E890, 0x20C,
        "C97FB1A95AEED81EBE76B98CF497020F6A92799D66330B15AE1F73112AEF77A0"),
    "WriteHeader": (0x0017EAA0, 0x244,
        "6975EB08BC74293ABCA884862D564DE1C2D2E243B2718DE5003E668A57E0CC13"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pc", type=Path,
        default=root / "local-data" / "pc-pristine" / "WinxClub.exe")
    parser.add_argument(
        "--ps2", type=Path,
        default=root / "local-data" / "Winx Club the game PS2" / "SLES_532.19")
    args = parser.parse_args()

    pc = args.pc.read_bytes()
    ps2 = args.ps2.read_bytes()
    pc_base, pc_sections = read_pe(pc)
    ps2_sections = read_elf(ps2)
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
    check("PS2 SHA-256", sha256(ps2), PS2_SHA256)
    check("PC image base", pc_base, 0x00400000)
    check(
        "PC exact source path",
        b"Z:\\Sparkplug\\Code\\Sparkplug\\spDataBlockSerializer.cpp\0" in pc,
        True,
    )
    check("PS2 source filename", b"spDataBlockSerializer.cpp\0" in ps2, True)

    for label, (address, size, expected) in PC_BODIES.items():
        body = image_slice(pc, pc_sections, address - pc_base, size)
        check(f"PC {label} body", digest(body), expected)
    for label, (address, size, expected) in PS2_BODIES.items():
        body = image_slice(ps2, ps2_sections, address, size)
        check(f"PS2 {label} body", digest(body), expected)

    pc_read = image_slice(pc, pc_sections, 0x004728F0 - pc_base, 0x1A9)
    check("PC ReadHeader low-ID mask", b"\x83\xe1\x1f" in pc_read, True)
    check("PC ReadHeader size-code shift", b"\xc1\xe8\x05" in pc_read, True)
    check("PC ReadHeader terminal ID", struct.pack("<I", 0xFFFFFFFF) in pc_read, True)
    for offset, pattern in (
        (0x0C, b"\x8d\x77\x0c"),
        (0x10, b"\xc7\x47\x10"),
        (0x14, b"\x89\x57\x14"),
        (0x18, b"\x89\x4f\x18"),
    ):
        check(f"PC header layout offset +0x{offset:02X}", pattern in pc_read, True)

    ps2_read = image_slice(ps2, ps2_sections, 0x0017E890, 0x20C)
    check("PS2 ReadHeader low-ID mask", struct.pack("<I", 0x3064001F) in ps2_read, True)
    check("PS2 ReadHeader size-code mask", struct.pack("<I", 0x306300E0) in ps2_read, True)
    check("PS2 ReadHeader size-code shift", struct.pack("<I", 0x00038143) in ps2_read, True)
    check("PS2 ReadHeader terminal ID", struct.pack("<I", 0x2402FFFF) in ps2_read, True)
    for word, label in (
        (0xAE44000C, "field ID +0x0C"),
        (0xAE420010, "payload size +0x10"),
        (0xAE420014, "header position +0x14"),
        (0xAE430018, "data position +0x18"),
    ):
        check(f"PS2 {label}", struct.pack("<I", word) in ps2_read, True)

    pc_skip = image_slice(pc, pc_sections, 0x00472AC0 - pc_base, 0x3D)
    check("PC SkipData reads size +0x04", b"\x8b\x40\x04" in pc_skip, True)
    check("PC SkipData reads data position +0x0C", b"\x8b\x50\x0c" in pc_skip, True)
    ps2_skip = image_slice(ps2, ps2_sections, 0x0017E830, 0x58)
    check("PS2 SkipData reads size +0x04", struct.pack("<I", 0x8CC20004) in ps2_skip, True)
    check("PS2 SkipData reads data position +0x0C", struct.pack("<I", 0x8CC3000C) in ps2_skip, True)
    check("PS2 SkipData uses essStart=1", struct.pack("<I", 0x24050001) in ps2_skip, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}"
    )
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
