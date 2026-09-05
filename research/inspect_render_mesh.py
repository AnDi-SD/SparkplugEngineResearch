#!/usr/bin/env python3
"""Read-only regression check for the storage-free spRenderMesh boundary."""

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
    "registration initializer": (
        0x006D5190,
        0x23,
        "198952D9E669CA4A54C253EA4E89B0D4C9A07C35AFE684C9506E180BB4146E7B",
    ),
    "registration getter": (
        0x004B1610,
        0x06,
        "522EE1B9AB28856FD4079081C4968BEEC0D5AA72D9FE1AA1ACC4294CE7A69F5E",
    ),
    "protected destructor and visible tail": (
        0x004B1620,
        0x12,
        "12CE6ED3BA46917A792DC999DB4A938B9599F0546FF3D1A78FD8826AFACC9CD3",
    ),
    "deleting destructor": (
        0x004B1640,
        0x1E,
        "28A96278662CE75664716B37B5EA393F21051BE29544700DCBE98E63C87FF826",
    ),
}

PS2_BODIES = {
    "registration initializer": (
        0x00482660,
        0x34,
        "05DD81A910492CEB3E5D257DE37BAB48CEBCB879E664439C78631478CDCE473D",
    ),
    "registration getter": (
        0x0015C340,
        0x0C,
        "72BDC3B53F7F8A914C0A53297A6E179A5332783CBF04C24AA162B10F052372FE",
    ),
    "deleting destructor": (
        0x0015C350,
        0x6C,
        "F3AD033F114F6F470161D590220D81D67F615EE62FC8DC70360562B8CA023730",
    ),
    "constructor": (
        0x0015C3C0,
        0x40,
        "19E19339493ECC9F008A6A48B55F939F5E3E571BDB201FFEBB6738352ABF9E56",
    ),
    "null clone": (
        0x0015C400,
        0x08,
        "008D26890102AF179C703D77FAE42CB7B3F424A11B8DB552383DD8E5313F4061",
    ),
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
    image_base, pc_sections = read_pe(pc)
    ps2_sections = read_elf(ps2)
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
    check("PC class string", b"spRenderMesh\0" in pc, True)
    check("PS2 class string", b"spRenderMesh\0" in ps2, True)

    for label, (address, size, expected) in PC_BODIES.items():
        body = image_slice(pc, pc_sections, address - image_base, size)
        check(f"PC {label}", digest(body), expected)
    for label, (address, size, expected) in PS2_BODIES.items():
        body = image_slice(ps2, ps2_sections, address, size)
        check(f"PS2 {label}", digest(body), expected)

    pc_initializer = image_slice(
        pc, pc_sections, 0x006D5190 - image_base, 0x23)
    check("PC class ID", struct.pack("<I", 0x67974A9C) in pc_initializer, True)
    check("PC direct spMesh base", struct.pack("<I", 0x3F077B6C) in pc_initializer, True)
    check("PC null factory and callback", pc_initializer[:4], b"\x6A\x00\x6A\x00")
    check(
        "PC primary vtable",
        struct.unpack(
            "<10I",
            image_slice(pc, pc_sections, 0x006EFFEC - image_base, 0x28)),
        (0x004B1640, 0x005B7A00, 0x004A1BF0, 0x00413120,
         0x004B1610, 0x00408350, 0x00408370, 0x00424360,
         0x0060DB76, 0x0060DB76),
    )
    check(
        "PC inherited secondary vtable",
        struct.unpack(
            "<2I",
            image_slice(pc, pc_sections, 0x006EFFE4 - image_base, 0x08)),
        (0x0060DB76, 0x0060DB76),
    )
    check(
        "PC destructor protected thunk",
        image_slice(pc, pc_sections, 0x004B1620 - image_base, 0x06),
        bytes.fromhex("FF 25 A8 15 3B 01"),
    )
    pc_destructor = image_slice(
        pc, pc_sections, 0x004B1620 - image_base, 0x12)
    check("PC destructor restores spMesh secondary vtable",
          struct.pack("<I", 0x006EFFE4) in pc_destructor, True)

    ps2_constructor = image_slice(ps2, ps2_sections, 0x0015C3C0, 0x40)
    check("PS2 constructor calls spMesh constructor",
          struct.pack("<I", 0x0C000000 | (0x00159EF0 >> 2)) in ps2_constructor, True)
    check("PS2 constructor primary vtable",
          bytes.fromhex("D0 E2 42 24") in ps2_constructor, True)
    check("PS2 constructor secondary vtable",
          bytes.fromhex("F4 E2 63 24") in ps2_constructor, True)
    check(
        "PS2 primary vtable header",
        struct.unpack("<9I", image_slice(ps2, ps2_sections, 0x0048E2D0, 0x24)),
        (0, 0, 0x0015C350, 0x00100810, 0x0015C400,
         0x00105DC0, 0x0015C340, 0x00100010, 0x00100050),
    )
    check(
        "PS2 secondary vtable header",
        struct.unpack("<7I", image_slice(ps2, ps2_sections, 0x0048E2F4, 0x1C)),
        (0, 0, 0, 0, 0x00159C80, 0, 0),
    )

    # The concrete PS2 subclass proves the base boundary: it allocates 0x58,
    # calls spRenderMesh's constructor, then starts its own fields at +0x50.
    ps2_derived_factory = image_slice(ps2, ps2_sections, 0x001EF6E0, 0x98)
    check("PS2 derived allocation size", bytes.fromhex("58 00 04 24") in ps2_derived_factory, True)
    check("PS2 derived calls spRenderMesh constructor",
          struct.pack("<I", 0x0C000000 | (0x0015C3C0 >> 2)) in ps2_derived_factory, True)
    check("PS2 first derived field +0x50 zeroed",
          bytes.fromhex("50 00 00 AE") in ps2_derived_factory, True)
    check("PS2 second derived field +0x54 zeroed",
          bytes.fromhex("54 00 00 AE") in ps2_derived_factory, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}"
    )
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
