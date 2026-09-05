#!/usr/bin/env python3
"""Read-only regression check for the PC-only spDXCombinedVB."""

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
    "registration initializer": (0x006D5550, 0x23, "C1BAFC06DC2F3D0E0800BC491E2F91D02A1888ECD5A38CF25EBE7CCE6442295D"),
    "destructor": (0x004C0C50, 0xF4, "299121A702019D4371216669E81FFEFE8493B63DC2D7E2FBB8E959A4BFCF6560"),
    "registration getter": (0x004C0D50, 0x06, "F096A86AC4BD4CC39C9E4773F51F5F2847EF80832C8AEB3725B8733B45DDE604"),
    "protected constructor thunk": (0x004C0D60, 0x06, "81D9954DAA63FBC5BDCF3D3336F4000D48F7D19C923666AFD630CF85CF7EAE12"),
    "deleting destructor": (0x004C0DF0, 0x1E, "E037E8DA45082AE2B798CC51421E15E3B76DAA740EE6FCEC2450D7A155FB00BB"),
    "protected factory thunk": (0x004C0E10, 0x06, "D31791A06CC76F78420787FE2BEAD3B72E8B7C80FE2EF5D851F917BF9B993E6F"),
    "clone": (0x004C0E70, 0x48, "E3E915159C14740993A9DCEEBD62ABD905862CE0BA83AAA66A02F5D83330C98C"),
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
    image_base, sections = read_pe(pc)
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
    check("PC class string", b"spDXCombinedVB\0" in pc, True)
    check("PS2 class string absent", b"spDXCombinedVB\0" in ps2, False)

    for label, (address, size, expected) in PC_BODIES.items():
        check(label, digest(image_slice(pc, sections, address - image_base, size)), expected)

    initializer = image_slice(pc, sections, 0x006D5550 - image_base, 0x23)
    check("class ID", struct.pack("<I", 0x4B18E622) in initializer, True)
    check("direct spBaseObject base", struct.pack("<I", 0x415352A1) in initializer, True)
    check("registration address", struct.pack("<I", 0x00764408) in initializer, True)
    check("protected factory address", struct.pack("<I", 0x004C0E10) in initializer, True)
    check(
        "vtable",
        struct.unpack("<7I", image_slice(pc, sections, 0x006F1E70 - image_base, 0x1C)),
        (0x004C0DF0, 0x005B7A00, 0x004C0E70, 0x0040ECE0,
         0x004C0D50, 0x00408350, 0x00408370),
    )

    destructor = image_slice(pc, sections, 0x004C0C50 - image_base, 0xF4)
    check("destructor restores class vtable",
          bytes.fromhex("C7 06 70 1E 6F 00") in destructor, True)
    check("destructor walks list sentinel +0x14",
          bytes.fromhex("8B 46 14 8B 38") in destructor, True)
    check("destructor frees index payload +0x30",
          bytes.fromhex("8B 46 30 50") in destructor, True)
    check("destructor frees vertex payload +0x2c",
          bytes.fromhex("8B 46 2C 50") in destructor, True)
    check("destructor tears down map state +0x1c",
          bytes.fromhex("8D 7E 1C") in destructor, True)
    check("destructor clears list size +0x18",
          bytes.fromhex("89 5E 18") in destructor, True)

    serializer_write = image_slice(pc, sections, 0x004C1ED0 - image_base, 0xC6)
    check("serializer reads vertex pointer +0x2c",
          bytes.fromhex("8B 76 2C") in serializer_write, True)
    check("serializer reads index pointer +0x30",
          bytes.fromhex("8B 4E 30") in serializer_write, True)
    check("serializer reads vertex size +0x34",
          bytes.fromhex("8B 46 34 50") in serializer_write, True)
    check("serializer reads index size +0x38",
          bytes.fromhex("8B 46 38") in serializer_write, True)
    source_id = image_slice(pc, sections, 0x004C1DD0 - image_base, 0x06)
    check("shared-data serializer selects this source class",
          struct.pack("<I", 0x4B18E622) in source_id, True)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
