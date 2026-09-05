#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for native spRenderNode evidence."""

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
        0x006D2A20, 0x26,
        "FFB8BB83ED384AA9368076EAC7BA81C654CA9704FB053CD1D6D6FB69E36D3EF1"),
    "protected factory thunk": (
        0x00425520, 0x06,
        "951A0D1F38A08EB8D39C38D19255072DBDD9059FD5AF9ADBA6237839043DF76B"),
    "registration getter": (
        0x00425030, 0x06,
        "D9146E6AD0956368CA1D5AD474C12163A120ED1CBF36A42E3948987C70D9BCC7"),
    "clone": (
        0x00425580, 0x48,
        "BD5FAD2B5464D5DF38BAD99EBCBE60D8A810B9D2C88F1B87592B83757C0C0A41"),
    "deleting destructor": (
        0x004255D0, 0x1E,
        "C07C38EC3B9C4C03CA7E6FE05EABB396E16C3FADE42C46315DD9B25942279B2D"),
}

PS2_BODIES = {
    "registration initializer": (
        0x00483F00, 0xAC,
        "2648C7EC9F29BA227D4F6F4CEE23EFFDD2B9EBA75ACA5201A2BE36A2B7DBDF7A"),
    "factory": (
        0x001AB160, 0xD8,
        "DD9197E913B7B330C85F6723C282959421BC385F777925247AA5FAF174E43BAD"),
    "constructor": (
        0x001AAFF0, 0x9C,
        "2A3A8F9CF4A4A4DF95D7ACC76EF6D04C985C252837D926AB1D5E69822FD9D19D"),
    "deleting destructor": (
        0x001AAEF0, 0x100,
        "525CE44568D0556E0867FFA3CD4AC1533805B76636CA5889803FEDF5A772D477"),
    "clone": (
        0x001AB090, 0xC4,
        "E0B3773D8DE967BEC90F73C291F20555968ECB9CF5F5D37FD8FEF4896A13BC11"),
    "copy": (
        0x001AA230, 0x90,
        "AE11FABD7478F6A211051B1E685595FA5E03AA6E1E6548F157284E62332AED2C"),
    "registration getter": (
        0x001A9DE0, 0x0C,
        "6D112CD2685827F785350524B6B7EF2FAF0B4975293F66F868E90F3E518DDFDE"),
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
    check("PC class string", b"spRenderNode\0" in pc, True)
    check("PS2 class string", b"spRenderNode\0" in ps2, True)

    for label, (address, size, expected) in PC_BODIES.items():
        check(
            f"PC {label}",
            digest(image_slice(
                pc, pc_sections, address - image_base, size)),
            expected)
    for label, (address, size, expected) in PS2_BODIES.items():
        check(
            f"PS2 {label}",
            digest(image_slice(ps2, ps2_sections, address, size)),
            expected)

    pc_initializer = image_slice(
        pc, pc_sections, 0x006D2A20 - image_base, 0x26)
    check("PC class ID", struct.pack("<I", 0x603625D0) in pc_initializer, True)
    check("PC direct spNode base", struct.pack("<I", 0x695C0F65) in pc_initializer, True)
    check("PC registration object", struct.pack("<I", 0x0075E150) in pc_initializer, True)
    check("PC protected factory", struct.pack("<I", 0x00425520) in pc_initializer, True)
    check(
        "PC primary vtable",
        struct.unpack(
            "<20I", image_slice(
                pc, pc_sections, 0x006DCAA4 - image_base, 0x50)),
        (0x004255D0, 0x00420B40, 0x00425580, 0x00424980,
         0x00425030, 0x00408350, 0x00408370, 0x00424760,
         0x004249F0, 0x00424AF0, 0x00420610, 0x00421330,
         0x004250F0, 0x00424E70, 0x00424B60, 0x004248D0,
         0x00424C30, 0x00425040, 0x00424790, 0x004247B0),
    )
    optimize_node = image_slice(
        pc, pc_sections, 0x004C19D0 - image_base, 0x238)
    check("PC optimizer reads renderable begin +0xbc",
          bytes.fromhex("8B 8E BC 00 00 00") in optimize_node, True)
    check("PC optimizer reads renderable end +0xc0",
          bytes.fromhex("8B 86 C0 00 00 00") in optimize_node, True)

    ps2_initializer = image_slice(ps2, ps2_sections, 0x00483F00, 0xAC)
    check("PS2 class ID", struct.pack("<I", 0x346525D0) in ps2_initializer, True)
    check("PS2 direct spNode base", struct.pack("<I", 0x34460F65) in ps2_initializer, True)
    check("PS2 concrete factory", struct.pack("<I", 0x2529B160) in ps2_initializer, True)
    ps2_factory = image_slice(ps2, ps2_sections, 0x001AB160, 0xD8)
    check("PS2 exact 0x1e0 allocation",
          bytes.fromhex("E0 01 04 24") in ps2_factory, True)
    check("PS2 16-byte allocation alignment",
          bytes.fromhex("10 00 05 24") in ps2_factory, True)
    check("PS2 renderable support subobject +0xc8",
          bytes.fromhex("C8 00 04 26") in ps2_factory, True)
    check("PS2 first matrix block +0x140",
          bytes.fromhex("40 01 04 26") in ps2_factory, True)
    check("PS2 second matrix block +0x180",
          bytes.fromhex("80 01 04 26") in ps2_factory, True)
    check("PS2 self pointer +0x134",
          bytes.fromhex("34 01 10 AE") in ps2_factory, True)
    check(
        "PS2 primary vtable header",
        struct.unpack(
            "<16I", image_slice(ps2, ps2_sections, 0x00490370, 0x40)),
        (0, 0, 0x001AAEF0, 0x001A5AC0, 0x001AB090, 0x001AA230,
         0x001A9DE0, 0x00100010, 0x00100050, 0x001AAEA0,
         0x001AACA0, 0x001AABF0, 0x001A7160, 0x001A7130,
         0x001AA2C0, 0x001A9F30),
    )
    check(
        "PS2 secondary vtable header",
        struct.unpack(
            "<13I", image_slice(ps2, ps2_sections, 0x004903B0, 0x34)),
        (0, 0, 0x001AB2D0, 0x00135E10, 0x00135E00, 0x001AB2E0,
         0x00135E80, 0x00135E70, 0x001AAA20, 0x001AA810,
         0x001AA580, 0x001A9F00, 0x001A9DF0),
    )
    ps2_destructor = image_slice(ps2, ps2_sections, 0x001AAEF0, 0x100)
    check("PS2 destructor drains callback count +0x1d0",
          bytes.fromhex("D0 01 22 8E") in ps2_destructor, True)
    check("PS2 destructor tears down support +0xc8",
          struct.pack("<I", 0x0C000000 | (0x001ABFB0 >> 2)) in ps2_destructor,
          True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
