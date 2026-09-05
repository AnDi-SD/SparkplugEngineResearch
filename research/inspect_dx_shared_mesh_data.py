#!/usr/bin/env python3
"""Read-only evidence check for PC spDXSharedMeshData."""

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
    "registration getter": (0x004C27F0, 0x06, "CC8D18CCA50D1FDDFC06E4FF56F0D58926B381833142B4EA22FA0A580E5C8ECE"),
    "destructor": (0x004C2800, 0xDA, "A43AD4B80C1C2202E5D0A7D918FF308F6C49F75CCFF8F99EFE720F34476EE345"),
    "protected factory": (0x004C28E0, 0x06, "385A02D5A89B8BDAAC13DC2A0110CC9072C8080417049C2843F297F6DF16B0D6"),
    "clone": (0x004C2950, 0x49, "A409A65CA3CF4F2795F98CA7DAE5165A8395092D611150CAF092B72D8C191F14"),
    "deleting destructor": (0x004C29A0, 0x1E, "DDCED2F1ADB6D70A1DB6603B48582CC31B16E8500E7EFF077A45DCD21EC6D0F6"),
    "buffer initialization": (0x004C29C0, 0x231, "41A0C626F9C2A6D4A42016E98008D0E23A31937FB5B097BF361B3F2BC4FDF708"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pc", type=Path, default=root / "local-data" / "pc-pristine" / "WinxClub.exe")
    parser.add_argument("--ps2", type=Path, default=root / "local-data" / "Winx Club the game PS2" / "SLES_532.19")
    args = parser.parse_args()

    pc = args.pc.read_bytes()
    ps2 = args.ps2.read_bytes()
    image_base, pc_sections = read_pe(pc)
    read_elf(ps2)
    checks = 0
    failures: list[str] = []

    def check(label: str, actual: object, expected: object) -> None:
        nonlocal checks
        checks += 1
        ok = actual == expected
        print(f"{'OK' if ok else 'FAIL':4} {label}: {actual!r}")
        if not ok:
            failures.append(f"{label}: expected {expected!r}, got {actual!r}")

    check("PC SHA-256", sha256(pc), PC_SHA256)
    check("PS2 SHA-256", sha256(ps2), PS2_SHA256)
    check("PC exact source path", b"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXSharedMeshData.cpp\0" in pc, True)
    check("PC class name", b"spDXSharedMeshData\0" in pc, True)
    check("PS2 class absent", b"spDXSharedMeshData\0" in ps2, False)

    for label, (address, size, expected) in PC_BODIES.items():
        check(label, digest(image_slice(pc, pc_sections, address - image_base, size)), expected)

    check(
        "primary vtable",
        struct.unpack("<7I", image_slice(pc, pc_sections, 0x006F23A0 - image_base, 0x1C)),
        (0x004C29A0, 0x005B7A00, 0x004C2950, 0x00413120, 0x004C27F0, 0x00408350, 0x00408370),
    )
    init = image_slice(pc, pc_sections, 0x004C29C0 - image_base, 0x231)
    check("allocates index wrapper 0x1c", bytes.fromhex("6A 1C") in init, True)
    check("allocates vertex wrapper 0x20", bytes.fromhex("6A 20") in init, True)
    check("INDEX16 format", bytes.fromhex("6A 65 6A 08") in init, True)
    check("stores index wrapper at +0x14", bytes.fromhex("89 73 14") in init, True)
    check("uses vertex slot at +0x18", bytes.fromhex("83 C3 18") in init, True)
    check("copies both complete payloads", init.count(bytes.fromhex("F3 A5")), 2)
    check("calls index init once", init.count(bytes.fromhex("E8 FB F6 FE FF")), 1)
    check("calls vertex init once", init.count(bytes.fromhex("E8 2A F4 FE FF")), 1)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
