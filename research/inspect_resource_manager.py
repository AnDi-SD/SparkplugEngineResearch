#!/usr/bin/env python3
"""Read-only regression check for native spResourceManager evidence."""

from __future__ import annotations

import argparse
import hashlib
import struct
from dataclasses import dataclass
from pathlib import Path


PC_SHA256 = "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F"
PS2_SHA256 = "198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE"


@dataclass(frozen=True)
class Section:
    address: int
    offset: int
    size: int
    executable: bool


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def read_pe(data: bytes) -> tuple[int, list[Section]]:
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    section_count = struct.unpack_from("<H", data, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    optional = pe_offset + 24
    image_base = struct.unpack_from("<I", data, optional + 28)[0]
    section_table = optional + optional_size
    sections: list[Section] = []
    for index in range(section_count):
        header = section_table + index * 40
        _, address, raw_size, raw_offset = struct.unpack_from(
            "<IIII", data, header + 8
        )
        characteristics = struct.unpack_from("<I", data, header + 36)[0]
        sections.append(
            Section(
                address,
                raw_offset,
                min(raw_size, max(0, len(data) - raw_offset)),
                bool(characteristics & 0x20000000),
            )
        )
    return image_base, sections


def read_elf(data: bytes) -> list[Section]:
    if data[:4] != b"\x7fELF" or data[4:6] != b"\x01\x01":
        raise ValueError("not a little-endian ELF32 image")
    table_offset = struct.unpack_from("<I", data, 32)[0]
    entry_size, count = struct.unpack_from("<HH", data, 46)
    sections: list[Section] = []
    for index in range(count):
        header = table_offset + index * entry_size
        _, _, flags, address, offset, size = struct.unpack_from(
            "<IIIIII", data, header
        )
        if size and offset < len(data):
            sections.append(
                Section(address, offset, min(size, len(data) - offset), bool(flags & 4))
            )
    return sections


def image_slice(data: bytes, sections: list[Section], address: int, size: int) -> bytes:
    for section in sections:
        relative = address - section.address
        if 0 <= relative and relative + size <= section.size:
            start = section.offset + relative
            return data[start : start + size]
    raise ValueError(f"address 0x{address:08X} is not file-backed")


def x86_call_count(data: bytes, sections: list[Section], target_rva: int) -> int:
    count = 0
    for section in sections:
        if not section.executable:
            continue
        raw = data[section.offset : section.offset + section.size]
        for offset in range(len(raw) - 4):
            if raw[offset] != 0xE8:
                continue
            displacement = struct.unpack_from("<i", raw, offset + 1)[0]
            source = section.address + offset
            count += source + 5 + displacement == target_rva
    return count


def mips_jal_word(target: int) -> int:
    return (3 << 26) | ((target >> 2) & 0x03FFFFFF)


def mips_jal_count(data: bytes, sections: list[Section], target: int) -> int:
    encoded = struct.pack("<I", mips_jal_word(target))
    return sum(
        data[section.offset : section.offset + section.size].count(encoded)
        for section in sections
        if section.executable
    )


def mips_lui_addiu_value(lui_word: int, addiu_word: int) -> int:
    """Resolve a LUI plus signed ADDIU immediate pair."""
    low = addiu_word & 0xFFFF
    signed_low = low - 0x10000 if low & 0x8000 else low
    return (((lui_word & 0xFFFF) << 16) + signed_low) & 0xFFFFFFFF


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pc",
        type=Path,
        default=repo_root / "local-data" / "pc-pristine" / "WinxClub.exe",
    )
    parser.add_argument(
        "--ps2",
        type=Path,
        default=repo_root / "local-data" / "Winx Club the game PS2" / "SLES_532.19",
    )
    args = parser.parse_args()

    pc = args.pc.read_bytes()
    ps2 = args.ps2.read_bytes()
    pc_base, pc_sections = read_pe(pc)
    ps2_sections = read_elf(ps2)
    failures: list[str] = []
    checks_run = 0

    def check(label: str, actual: object, expected: object) -> None:
        nonlocal checks_run
        checks_run += 1
        status = "OK" if actual == expected else "FAIL"
        print(f"{status:4} {label}: {actual!r}")
        if actual != expected:
            failures.append(f"{label}: expected {expected!r}, got {actual!r}")

    check("PC SHA-256", sha256(pc), PC_SHA256)
    check("PS2 SHA-256", sha256(ps2), PS2_SHA256)
    check("PC image base", pc_base, 0x00400000)
    check("PC class name", b"spResourceManager\0" in pc, True)
    check("PS2 class name", b"spResourceManager\0" in ps2, True)

    # PC initializer RVA 0x2d3640: property registrar, factory, base record,
    # class name, direct-base ID, class ID, registration-record address.
    pc_init = image_slice(pc, pc_sections, 0x002D3640, 33)
    check("PC initializer factory", struct.unpack_from("<I", pc_init, 3)[0], 0x00458D00)
    check("PC initializer base record", struct.unpack_from("<I", pc_init, 8)[0], 0x00755310)
    check("PC initializer class name", struct.unpack_from("<I", pc_init, 13)[0], 0x006E7058)
    check("PC initializer base ID", struct.unpack_from("<I", pc_init, 18)[0], 0x415352A1)
    check("PC initializer class ID", struct.unpack_from("<I", pc_init, 23)[0], 0xA4B9923B)
    check("PC initializer registration", struct.unpack_from("<I", pc_init, 28)[0], 0x0075FB78)
    check(
        "PC protected factory thunk",
        image_slice(pc, pc_sections, 0x00058D00, 6),
        bytes.fromhex("ff25cc2f3b01"),
    )
    check(
        "PC primary vtable",
        struct.unpack("<7I", image_slice(pc, pc_sections, 0x002E703C, 28)),
        (0x00458C90, 0x005B7A00, 0x00458D60, 0x0040ECE0,
         0x00458B00, 0x00408350, 0x00408370),
    )
    pc_destructor = image_slice(pc, pc_sections, 0x00058AB0, 0x50)
    check("PC destructor clears vector words", bytes.fromhex("897e24897e28897e2c") in pc_destructor, True)
    check("PC destructor clears singleton", bytes.fromhex("893d78db7500") in pc_destructor, True)
    check("PC register callers", x86_call_count(pc, pc_sections, 0x00058CB0), 2)
    check("PC destructor caller", x86_call_count(pc, pc_sections, 0x00058AB0), 1)

    ps2_init = struct.unpack(
        "<14I", image_slice(ps2, ps2_sections, 0x00482FD0, 14 * 4)
    )
    check("PS2 initializer class ID", ((ps2_init[1] & 0xFFFF) << 16) | (ps2_init[7] & 0xFFFF), 0xA4B9923B)
    check("PS2 initializer base ID", ((ps2_init[2] & 0xFFFF) << 16) | (ps2_init[8] & 0xFFFF), 0x415352A1)
    check("PS2 initializer registration", mips_lui_addiu_value(ps2_init[0], ps2_init[6]), 0x004A9D30)
    check("PS2 initializer class name", mips_lui_addiu_value(ps2_init[3], ps2_init[9]), 0x00448DE0)
    check("PS2 initializer factory", mips_lui_addiu_value(ps2_init[5], ps2_init[11]), 0x0017DD80)

    check(
        "PS2 primary vtable",
        struct.unpack("<9I", image_slice(ps2, ps2_sections, 0x0048EEF0, 36)),
        (0, 0, 0x0017DB10, 0x00100810, 0x0017DC40, 0x00100320,
         0x0017D0B0, 0x00100010, 0x00100050),
    )
    factory = struct.unpack(
        "<40I", image_slice(ps2, ps2_sections, 0x0017DD80, 40 * 4)
    )
    check("PS2 exact allocation", factory[3], 0x2404002C)
    check("PS2 main vtable store", factory[24], 0xAE020000)
    check("PS2 support vtable store", factory[27], 0xAE020010)
    check("PS2 reserve flag default", factory[28], 0xA2000015)
    check("PS2 reserve count default", factory[30], 0xAE000018)
    check("PS2 field +1c default", (factory[29], factory[32]), (0x2402FFFF, 0xAE02001C))
    check(
        "PS2 singleton publication",
        (factory[20], factory[21]),
        (0x3C020049, 0xAF83B6F8),
    )

    configure = image_slice(ps2, ps2_sections, 0x0017D360, 0x34)
    check("PS2 Configure stores flag +0x15", struct.pack("<I", 0xA0850015) in configure, True)
    check("PS2 Configure stores count +0x18", struct.pack("<I", 0xAC860018) in configure, True)
    game_configure = struct.unpack(
        "<4I", image_slice(ps2, ps2_sections, 0x00277AEC, 16)
    )
    check(
        "PS2 game startup reserve request",
        game_configure,
        (0x8F84B6F8, 0x24050001, mips_jal_word(0x0017D360), 0x240609C4),
    )

    check("PS2 Configure caller count", mips_jal_count(ps2, ps2_sections, 0x0017D360), 1)
    check("PS2 Remove caller count", mips_jal_count(ps2, ps2_sections, 0x0017D3A0), 1)
    check("PS2 Find caller count", mips_jal_count(ps2, ps2_sections, 0x0017D480), 2)
    check("PS2 Register caller count", mips_jal_count(ps2, ps2_sections, 0x0017D670), 2)
    resource_destructor = image_slice(ps2, ps2_sections, 0x0017CEB0, 0x90)
    check(
        "PS2 resource destructor lazy manager factory",
        struct.pack("<I", mips_jal_word(0x0017DD80)) in resource_destructor,
        True,
    )
    check(
        "PS2 resource destructor unregister",
        struct.pack("<I", mips_jal_word(0x0017D3A0)) in resource_destructor,
        True,
    )

    find_body = image_slice(ps2, ps2_sections, 0x0017D480, 0x1F0)
    register_body = image_slice(ps2, ps2_sections, 0x0017D670, 0x170)
    for label, body, register_form in (
        ("Find", find_body, False),
        ("Register", register_body, True),
    ):
        register_delta = 0x00010000 if register_form else 0
        check(f"PS2 {label} texture class ID", struct.pack("<I", 0x3C022F28 + register_delta) in body and struct.pack("<I", 0x34451E13 + 0x00200000 * register_form) in body, True)
        check(f"PS2 {label} mesh class ID", struct.pack("<I", 0x3C023F07 + register_delta) in body and struct.pack("<I", 0x34457B6C + 0x00200000 * register_form) in body, True)
    check("PS2 Find texture category", struct.pack("<I", 0x24020001) in find_body, True)
    check("PS2 Find mesh category", struct.pack("<I", 0x24020002) in find_body, True)
    check("PS2 Register unsupported category", struct.pack("<I", 0x24030010) in register_body, True)

    load_resource = image_slice(ps2, ps2_sections, 0x0017D7E0, 0x240)
    load_scene = image_slice(ps2, ps2_sections, 0x0017DA20, 0xF0)
    check("PS2 .stx texture shortcut spelling", b".stx\0" in ps2, True)
    check("PS2 load invokes serializer-manager generic load", struct.pack("<I", mips_jal_word(0x00182640)) in load_resource, True)
    check("PS2 scene load invokes serializer-manager scene load", struct.pack("<I", mips_jal_word(0x00182150)) in load_scene, True)
    check("PS2 scene open diagnostic", b"Can't open file: %s\0" in ps2, True)

    materialize = image_slice(ps2, ps2_sections, 0x00182B90, 0x328)
    materialize_dependencies = [
        target
        for target in (
            0x0017F8A0,  # first FAT entry
            0x0017F840,  # next FAT entry
            0x0017D480,  # resource-manager cache lookup
            0x0017D670,  # cache newly loaded named resource
            0x001810F0,  # apply FAT name
            0x0017FA10,  # external-file lookup
        )
        if struct.pack("<I", mips_jal_word(target)) in materialize
    ]
    check(
        "PS2 FAT materialization dependency chain",
        materialize_dependencies,
        [0x0017F8A0, 0x0017F840, 0x0017D480, 0x0017D670, 0x001810F0, 0x0017FA10],
    )
    check(
        "PS2 FAT materialization has two lazy manager sites",
        materialize.count(struct.pack("<I", mips_jal_word(0x0017DD80))),
        2,
    )
    check(
        "PS2 FAT cache hit is stored at entry +0x20",
        struct.pack("<I", 0xAE220020) in materialize,
        True,
    )
    check(
        "PS2 FAT materialization tests entry file ID +0x08",
        struct.pack("<I", 0x8E250008) in materialize,
        True,
    )
    check(
        "PS2 newly loaded named objects gate on spNamedObject",
        struct.pack("<I", 0x3C0244DE) in materialize
        and struct.pack("<I", 0x344507FD) in materialize,
        True,
    )

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks_run - len(failures)}/{checks_run}"
    )
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
