#!/usr/bin/env python3
"""Read-only evidence check for PC spDXSharedMeshDataSerializer."""

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
    "registration getter": (0x004C1DA0, 0x06, "4476EEA9FBEFCC68DCBD9E3C5A6BAC7FD566938B5FAB0CEE5AAD1B7F218E7971"),
    "destructor": (0x004C1DB0, 0x12, "5AABE04F7395CA7B9E6E958ECE76D08DA310D8B9240F38F09EC1A59DDAC69314"),
    "source class ID": (0x004C1DD0, 0x06, "F2659DF1A5FC4F1AE1B0218822685A5DCE55051A34066F158A412B7767EBFA2B"),
    "resolved target class ID": (0x004C1DE0, 0x08, "7326BB011FAC49D838E913FEF67DFC64C2CC11FA714CB0409B4199FD4D1032DB"),
    "protected factory": (0x004C1DF0, 0x06, "CE4205DBA980E512E720E3BFD3B10E4453F6FD244A1EED8F0D1400234CBBEABA"),
    "clone": (0x004C1E60, 0x49, "2FC166A1AD29D5C97230C8C456DD1C054F980A1474799A76137AC8A03CBDCFEC"),
    "deleting destructor": (0x004C1EB0, 0x1E, "85B93682C8D4EFF436689F21338DA94C3B6FAC7130BEBE115A07146410DAE4AA"),
    "write": (0x004C1ED0, 0xC6, "20E4C53B3CED632E6A38538906C6EE8551F5D05ADE329749AD23FA6CFE0CAA52"),
    "read": (0x004C1FA0, 0xDA, "6851846BFDFE5C5B42D849C3C093359C64E33F3146EA5BC4C334D39F622AE857"),
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
    check("PC exact shared source path", b"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXSharedMeshData.cpp\0" in pc, True)
    check("PC serializer class name", b"spDXSharedMeshDataSerializer\0" in pc, True)
    check("PS2 serializer absent", b"spDXSharedMeshDataSerializer\0" in ps2, False)

    for label, (address, size, expected) in PC_BODIES.items():
        check(label, digest(image_slice(pc, pc_sections, address - image_base, size)), expected)

    check(
        "primary vtable",
        struct.unpack("<11I", image_slice(pc, pc_sections, 0x006F2108 - image_base, 0x2C)),
        (0x004C1EB0, 0x005B7A00, 0x004C1E60, 0x0040ECE0,
         0x004C1DA0, 0x00408350, 0x00408370, 0x00467550,
         0x004C1DE0, 0x004672C0, 0x004C1DD0),
    )
    check(
        "stream interface vtable",
        struct.unpack("<3I", image_slice(pc, pc_sections, 0x006F20FC - image_base, 0x0C)),
        (0x004C1ED0, 0x005A7DB0, 0x004C1FA0),
    )

    source_id = image_slice(pc, pc_sections, 0x004C1DD0 - image_base, 6)
    target_id = image_slice(pc, pc_sections, 0x004C1DE0 - image_base, 8)
    check("returns spDXCombinedVB source ID", struct.pack("<I", 0x4B18E622) in source_id, True)
    check("maps source ID to shared mesh target", struct.pack("<I", 0x293A2681) in target_id, True)

    write = image_slice(pc, pc_sections, 0x004C1ED0 - image_base, 0xC6)
    check("write reads index size +0x38 first", write[:9], bytes.fromhex("56 8B 74 24 0C 8B 46 38 57"))
    check("write then reads vertex size +0x34", bytes.fromhex("8B 46 34 50") in write, True)
    check("write consumes index pointer +0x30", bytes.fromhex("8B 4E 30") in write, True)
    check("write consumes vertex pointer +0x2c", bytes.fromhex("8B 76 2C") in write, True)
    check("write performs two raw stream writes", write.count(bytes.fromhex("FF 52 38")), 2)

    read = image_slice(pc, pc_sections, 0x004C1FA0 - image_base, 0xDA)
    check("read obtains current stream position", bytes.fromhex("FF 52 2C") in read, True)
    check("read obtains raw stream buffer through both register forms",
          read.count(bytes.fromhex("FF 52 40"))
          + read.count(bytes.fromhex("FF 50 40")), 2)
    check("read calls shared mesh initializer", bytes.fromhex("E8 4E 09 00 00") in read, True)
    check("native read returns true after Init call", read[-13:],
          bytes.fromhex("E8 4E 09 00 00 5E B0 01 5F 59 C2 08 00"))

    print(f"RESULT {'PASS' if not failures else 'FAIL'} checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
