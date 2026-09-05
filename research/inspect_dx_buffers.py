#!/usr/bin/env python3
"""Read-only regression check for the PC spDX vertex/index wrappers."""

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
    x86_call_count,
)


PC_BODIES = {
    "vertex registration initializer": (
        0x006D51F0, 0x26,
        "B60306EA198FD7388BC2DA70ECF0F9C38C791FEBB240463D1C5AA775E7919173"),
    "vertex constructor protected entry": (
        0x004B1E00, 0x06,
        "6F672DB41EADC11FCF870ADEC9FD2D7E3A0571B09BEE873A629F238422059E4C"),
    "vertex factory protected entry": (
        0x004B1E30, 0x06,
        "63ECE0A1C744258E1A1F226CD8FEDB2B77E1876B000F596B72F8C659A9E585F2"),
    "vertex clone": (
        0x004B1EA0, 0x49,
        "63113056ECA181DCE278D4F671F36607E8DB7E2677AE44F09AF8CB69DD5D6CE0"),
    "vertex destructor": (
        0x004B1EF0, 0x62,
        "70E8BD76347A00831596E053483C6DA0DAC6582FAB757600133AEDF82DF9F5A2"),
    "vertex initialize": (
        0x004B1F60, 0x3E,
        "59CEDF74A85F03854C2069B30897C6A06F9F2819B0CB57805ABBA88A5F80A500"),
    "vertex deleting destructor": (
        0x004B1FA0, 0x1E,
        "2D28E7E6242E8B95C88E5C358AF957678F06B8AFE6D3F465722891BDC3517881"),
    "index registration initializer": (
        0x006D5220, 0x26,
        "F2002231F8938C29BD680A6CA332A3F8FBC401A2808E7C4FE081E19AE50F04CF"),
    "index constructor protected entry": (
        0x004B1FC0, 0x06,
        "647492FBB9681D35ABB7EEBBAA557E61C4B20D925A9A3D4EC135A6E4CAF538AA"),
    "index release": (
        0x004B1FF0, 0x1F,
        "BC07EEAE950A1C55C969BFCBB33FCE157BB960A1B38804025B5FDF07305CB489"),
    "index factory protected entry": (
        0x004B2010, 0x06,
        "F4A59B5A6B8FE521BA2CEE7A701F061F6DCB3A65F31631DC9FA94104F0533C3B"),
    "index clone": (
        0x004B2080, 0x49,
        "9B88CF5C28B8A9E1595091257562B4DCFCE7A48A23A44D37ED3A65A3B53904C9"),
    "index destructor": (
        0x004B20D0, 0x62,
        "396B4889BD008C5970AD72F60663D8517D6D62E2E72869DD257DFA404DDB5409"),
    "index initialize": (
        0x004B2140, 0x39,
        "FED7470DA4CE1EA29FEF80EDE7780C73BF78FE7A9C841C86EBA3CFFC86D3C58B"),
    "index deleting destructor": (
        0x004B2180, 0x1E,
        "A808286E2E2AE0AF5E12C7662F564AAF955F23A72811F811332986310EC3A327"),
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
    del ps2_sections
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
    for class_name in (b"spDXVertexBuffer\0", b"spDXIndexBuffer\0"):
        printable = class_name[:-1].decode("ascii")
        check(f"PC {printable} string", class_name in pc, True)
        check(f"PS2 {printable} absent", class_name in ps2, False)

    for label, (address, size, expected) in PC_BODIES.items():
        body = image_slice(pc, pc_sections, address - image_base, size)
        check(f"PC {label}", digest(body), expected)

    vertex_registration = image_slice(
        pc, pc_sections, 0x006D51F0 - image_base, 0x26)
    index_registration = image_slice(
        pc, pc_sections, 0x006D5220 - image_base, 0x26)
    check("vertex class ID", struct.pack("<I", 0x37036C17) in vertex_registration, True)
    check("index class ID", struct.pack("<I", 0x23022413) in index_registration, True)
    check("vertex direct base", struct.pack("<I", 0x415352A1) in vertex_registration, True)
    check("index direct base", struct.pack("<I", 0x415352A1) in index_registration, True)
    check("vertex RTTI factory", struct.pack("<I", 0x004B1E30) in vertex_registration, True)
    check("index RTTI factory", struct.pack("<I", 0x004B2010) in index_registration, True)

    check(
        "vertex vtable",
        struct.unpack("<7I", image_slice(
            pc, pc_sections, 0x006F05C4 - image_base, 0x1C)),
        (0x004B1FA0, 0x005B7A00, 0x004B1EA0, 0x0040ECE0,
         0x004B1E20, 0x00408350, 0x00408370),
    )
    check(
        "index vtable",
        struct.unpack("<7I", image_slice(
            pc, pc_sections, 0x006F05F8 - image_base, 0x1C)),
        (0x004B2180, 0x005B7A00, 0x004B2080, 0x0040ECE0,
         0x004B1FE0, 0x00408350, 0x00408370),
    )

    vertex_initialize = image_slice(
        pc, pc_sections, 0x004B1F60 - image_base, 0x3E)
    index_initialize = image_slice(
        pc, pc_sections, 0x004B2140 - image_base, 0x39)
    check("CreateVertexBuffer vtable call", b"\xFF\x51\x68" in vertex_initialize, True)
    check("CreateIndexBuffer vtable call", b"\xFF\x51\x6C" in index_initialize, True)
    check("vertex stores FVF at +0x14", b"\x89\x7E\x14" in vertex_initialize, True)
    check("vertex stores byte size at +0x1C", b"\x89\x5E\x1C" in vertex_initialize, True)
    check("index stores byte size at +0x18", b"\x89\x7E\x18" in index_initialize, True)

    check("vertex constructor direct callers",
          x86_call_count(pc, pc_sections, 0x004B1E00 - image_base), 5)
    check("vertex initialize direct callers",
          x86_call_count(pc, pc_sections, 0x004B1F60 - image_base), 5)
    check("index constructor direct callers",
          x86_call_count(pc, pc_sections, 0x004B1FC0 - image_base), 5)
    check("index initialize direct callers",
          x86_call_count(pc, pc_sections, 0x004B2140 - image_base), 6)
    check("index release direct callers",
          x86_call_count(pc, pc_sections, 0x004B1FF0 - image_base), 11)

    combiner_initialize = image_slice(
        pc, pc_sections, 0x004A96C0 - image_base, 0x21E)
    check("combiner allocates 0x1C-byte index wrapper",
          b"\x6A\x1C" in combiner_initialize, True)
    check("combiner allocates 0x20-byte vertex wrapper",
          b"\x6A\x20" in combiner_initialize, True)
    check("combiner requests INDEX16/usage 8/pool 1",
          bytes.fromhex("6A 01 6A 65 6A 08 53 E8") in combiner_initialize, True)
    check("combiner requests vertex usage 8/pool 1",
          bytes.fromhex("6A 01 52 6A 08 50 E8") in combiner_initialize, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}"
    )
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
