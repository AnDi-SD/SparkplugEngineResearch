#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for native spRenderNodeSerializer."""

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
    "registration initializer": (0x006D3AF0, 0x26, "5C1E3EFB6883AB4E0AB7BD40400AEBBA04F078F40EAA9D4BE52B754F402D9B25"),
    "registration getter": (0x00469000, 0x06, "870785BADEBE839CF5D818BAFCED396692D2A188C3899632346E46B2FE061461"),
    "target class ID": (0x00469030, 0x06, "8339B88AA7956F1616E0EBBDB75DA51F4DD5DE3CB34DAAF59AE387E5E274EDFC"),
    "protected factory": (0x00469040, 0x06, "E2BBC17047F1B2EB80EAFFF83E978651AA26577A9DC9F6A3F6914C8359ECD5C6"),
    "clone": (0x004690B0, 0x49, "36E343785BCC4BBFA3C1A0C183C3F36A9839F37F2B846E34FC541754F36085AC"),
    "deleting destructor": (0x00469100, 0x1E, "FA738C03F77D932655E2B80C8A87F68070E08FA16E33ECBB51689AA4782FC547"),
    "index relationships": (0x00469120, 0x67, "59E6641B59CB52AC7D31797BC5BB1DBABBDDB22933A0F904C47F98AF55ECA32F"),
    "read": (0x00469190, 0x1A7, "256FBFC52221FFA205561834832E2137C2F8E958FC5A29A7BCC13A5106B744D7"),
    "write": (0x00469340, 0x355, "76329FED03D7D2295837886BBE98F229FCC05A93A488B9B16465F87689D0A1A8"),
}

PS2_BODIES = {
    "registration initializer": (0x00483790, 0x38, "25B7E7490AF2AF35C32C263EB67B960EF5782F42565FCD0C57245291859EB36D"),
    "registration getter": (0x00197E30, 0x0C, "47EC9BC22D58883F722B76238778444EFD83C64CFE731E3E6F5E9A9407A52A0E"),
    "read": (0x00197E40, 0x1A4, "24BE954A58A05A24BC58900AEC19D84B3AA32DE14792BDA16E56C72D57C4721E"),
    "index relationships": (0x00197FF0, 0xAC, "87347481DED7A686A405F4C67E2E383A96DBFAAE05F79E8A39919956C9826AF2"),
    "write": (0x001980A0, 0x1DC, "7E9C226A1ECF45C5AB5DA0690CD14CC16C8B0022F7A0C1ADBF65A88287D2D5B9"),
    "target class ID": (0x00198280, 0x0C, "57256082D6334AB4A23865A9938671810C90A6DC955B64E646AF460C2D9B0F6F"),
    "deleting destructor": (0x00198290, 0x6C, "AB41040248744D25791163779757D76793F8B9D018771CA64C1EBD4573DB9577"),
    "constructor": (0x00198300, 0x40, "F19714C0454FA50E2A0C9EAD9A851A5312615AF0BF393D3D52324649F0A86212"),
    "clone": (0x00198340, 0xD8, "6045A4CF520A7FCDF3F5958F78C0E9A0AA8F2E18B4861E936DB3FE1DB5ECF6F8"),
    "factory": (0x00198420, 0x64, "768D91395010A1B4908FB024E58358E09A05E52C543301416AEC009FCDBB4D78"),
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
    check("PC class string", b"spRenderNodeSerializer\0" in pc, True)
    check("PS2 class string", b"spRenderNodeSerializer\0" in ps2, True)
    check("PC exact source path", b"Z:\\Sparkplug\\Code\\Sparkplug\\spRenderNodeSerializer.cpp\0" in pc, True)
    check("PS2 source filename", b"spRenderNodeSerializer.cpp\0" in ps2, True)

    for label, (address, size, expected) in PC_BODIES.items():
        check(f"PC {label}", digest(image_slice(pc, pc_sections, address - image_base, size)), expected)
    for label, (address, size, expected) in PS2_BODIES.items():
        check(f"PS2 {label}", digest(image_slice(ps2, ps2_sections, address, size)), expected)

    pc_initializer = image_slice(pc, pc_sections, 0x006D3AF0 - image_base, 0x26)
    for label, value in (
        ("class ID", 0x66EF6060),
        ("direct spNodeSerializer base ID", 0x4545848A),
        ("registration object", 0x007604C0),
        ("direct base registration", 0x007601C0),
        ("protected factory", 0x00469040),
    ):
        check(f"PC {label}", struct.pack("<I", value) in pc_initializer, True)
    check(
        "PC interface vtable",
        struct.unpack("<3I", image_slice(pc, pc_sections, 0x006E8A38 - image_base, 0x0C)),
        (0x00469340, 0x00469120, 0x00469190),
    )
    check(
        "PC primary vtable prefix",
        struct.unpack("<11I", image_slice(pc, pc_sections, 0x006E8A44 - image_base, 0x2C)),
        (0x00469100, 0x005B7A00, 0x004690B0, 0x0040ECE0,
         0x00469000, 0x00408350, 0x00408370, 0x00467550,
         0x004671E0, 0x004672C0, 0x00469030),
    )
    pc_read = image_slice(pc, pc_sections, 0x00469190 - image_base, 0x1A7)
    pc_index = image_slice(pc, pc_sections, 0x00469120 - image_base, 0x67)
    pc_write = image_slice(pc, pc_sections, 0x00469340 - image_base, 0x355)
    check("PC reader requires spRenderable", bytes.fromhex("68 42 45 DA 4F") in pc_read, True)
    check("PC reader attaches through support +0xb4", bytes.fromhex("8D 8D B4 00 00 00") in pc_read, True)
    check("PC index reads renderable begin +0xbc", bytes.fromhex("8B 8E BC 00 00 00") in pc_index, True)
    check("PC index reads renderable end +0xc0", bytes.fromhex("8B 86 C0 00 00 00") in pc_index, True)
    check("PC writer reads renderable begin +0xbc", bytes.fromhex("8B 96 BC 00 00 00") in pc_write, True)

    ps2_initializer = image_slice(ps2, ps2_sections, 0x00483790, 0x38)
    check("PS2 registration encodes class ID halves", all(p in ps2_initializer for p in (bytes.fromhex("EF 66 03 3C"), bytes.fromhex("60 60 65 34"))), True)
    check("PS2 registration encodes direct base ID halves", all(p in ps2_initializer for p in (bytes.fromhex("45 45 02 3C"), bytes.fromhex("8A 84 46 34"))), True)
    check("PS2 factory allocates exact 0x14", bytes.fromhex("14 00 04 24") in image_slice(ps2, ps2_sections, 0x00198420, 0x64), True)
    check(
        "PS2 primary vtable header",
        struct.unpack("<9I", image_slice(ps2, ps2_sections, 0x0048FA10, 0x24)),
        (0, 0, 0x00198290, 0x00100810, 0x00198340,
         0x00100320, 0x00197E30, 0x00100010, 0x00100050),
    )
    check(
        "PS2 interface vtable header",
        struct.unpack("<12I", image_slice(ps2, ps2_sections, 0x0048FA34, 0x30)),
        (0, 0, 0x001984B0, 0x001984A0, 0x00198490,
         0x001815B0, 0x001814D0, 0x00197FF0, 0x00181BE0,
         0x00198280, 0x001980A0, 0x00197E40),
    )
    ps2_read = image_slice(ps2, ps2_sections, 0x00197E40, 0x1A4)
    ps2_index = image_slice(ps2, ps2_sections, 0x00197FF0, 0xAC)
    ps2_write = image_slice(ps2, ps2_sections, 0x001980A0, 0x1DC)
    check("PS2 reader attaches through support +0xc8", bytes.fromhex("C8 00 04 26") in ps2_read, True)
    check("PS2 index reads count +0xd0", bytes.fromhex("D0 00 42 8E") in ps2_index, True)
    check("PS2 index reads storage +0xd4", bytes.fromhex("D4 00 42 8E") in ps2_index, True)
    check("PS2 writer emits field zero with size code seven", bytes.fromhex("2D 28 00 00 D8 FB 05 0C 07 00 06 24") in ps2_write, True)
    check("both readers diagnose forbidden null relationship", b"No empty rendernode->renderable (NULL renderable) relation allowed. File corrupt?" in pc and b"No empty rendernode->renderable (NULL renderable) relation allowed. File corrupt?" in ps2, True)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
