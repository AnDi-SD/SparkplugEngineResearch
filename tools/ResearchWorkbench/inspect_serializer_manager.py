#!/usr/bin/env python3
"""Read-only regression check for spSerializerManager/FAT binary evidence."""

from __future__ import annotations

import argparse
import hashlib
import struct
from collections import Counter
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
        _virtual_size, address, raw_size, raw_offset = struct.unpack_from(
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
    result: list[Section] = []
    for index in range(count):
        header = table_offset + index * entry_size
        _, _, flags, address, offset, size = struct.unpack_from(
            "<IIIIII", data, header
        )
        if size and offset < len(data):
            result.append(
                Section(address, offset, min(size, len(data) - offset), bool(flags & 4))
            )
    return result


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


def mips_registration_tuples(
    data: bytes, sections: list[Section], target: int
) -> list[tuple[int | None, int | None, int | None]]:
    target_word = mips_jal_word(target)
    result: list[tuple[int | None, int | None, int | None]] = []
    for section in sections:
        if not section.executable:
            continue
        raw = data[section.offset : section.offset + section.size]
        words = struct.unpack(f"<{len(raw) // 4}I", raw[: len(raw) & ~3])
        for call_index, call_word in enumerate(words):
            if call_word != target_word:
                continue
            registers: dict[int, int] = {0: 0}
            # The instruction immediately after JAL is executed in the MIPS
            # delay slot and commonly supplies t0 (the fifth argument).
            window = list(words[max(0, call_index - 20) : call_index])
            if call_index + 1 < len(words):
                window.append(words[call_index + 1])
            for word in window:
                opcode = word >> 26
                source = (word >> 21) & 31
                target_register = (word >> 16) & 31
                immediate = word & 0xFFFF
                if opcode == 0x0F:  # LUI
                    registers[target_register] = immediate << 16
                elif opcode == 0x0D:  # ORI
                    if source in registers:
                        registers[target_register] = registers[source] | immediate
                    else:
                        registers.pop(target_register, None)
                elif opcode == 0x09:  # ADDIU
                    if source in registers:
                        registers[target_register] = (
                            registers[source] + signed16(immediate)
                        ) & 0xFFFFFFFF
                    else:
                        registers.pop(target_register, None)
                elif opcode == 0x23:  # LW
                    registers.pop(target_register, None)
                elif opcode == 0 and (word & 0x3F) in (0x21, 0x25, 0x2D):
                    # ADDU/OR/DADDU; all three encode the MOVE pseudo-op when
                    # one source is zero.
                    right = (word >> 16) & 31
                    destination = (word >> 11) & 31
                    if source in registers and right in registers:
                        registers[destination] = (
                            registers[source] + registers[right]
                        ) & 0xFFFFFFFF
                    else:
                        registers.pop(destination, None)
                elif opcode == 3:  # JAL clobbers caller-saved registers
                    for register in range(2, 16):
                        registers.pop(register, None)
            result.append((registers.get(5), registers.get(7), registers.get(8)))
    return result


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pc",
        type=Path,
        default=repo_root / "local-data" / "pc-pristine" / "WinxClub.exe",
    )
    parser.add_argument(
        "--ps2",
        type=Path,
        default=(
            repo_root
            / "local-data"
            / "Winx Club the game PS2"
            / "SLES_532.19"
        ),
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
    check(
        "PC exact source path",
        b"Z:\\Sparkplug\\Code\\Sparkplug\\spSerializerManager.cpp\0" in pc,
        True,
    )
    check("PS2 source filename", b"spSerializerManager.cpp\0" in ps2, True)
    check(
        "PC FAT source path",
        b"Z:\\Sparkplug\\Code\\Sparkplug\\spResourceFATSerializer.cpp\0" in pc,
        True,
    )
    check("PS2 FAT source filename", b"spResourceFATSerializer.cpp\0" in ps2, True)
    for field_spelling in (
        b"pEntry->m_uFileID",
        b"pEntry->m_szFilename",
        b"pEntry->m_uID",
        b"pEntry->m_szName",
        b"pEntry->m_ClassID",
        b"pEntry->m_uOffset",
        b"pEntry->m_uSize",
    ):
        check(
            f"PS2 FAT field spelling {field_spelling.decode('ascii')}",
            field_spelling in ps2,
            True,
        )

    # PC static RTTI initializer at VA 0x006D2840. Decode literal push operands
    # instead of comparing the CALL displacement, which is less explanatory.
    init = image_slice(pc, pc_sections, 0x002D2840, 32)
    check("PC image base", pc_base, 0x00400000)
    check("PC manager initializer factory", struct.unpack_from("<I", init, 3)[0], 0x00422F40)
    check("PC manager initializer base record", struct.unpack_from("<I", init, 8)[0], 0x00755310)
    check("PC manager initializer base ID", struct.unpack_from("<I", init, 18)[0], 0x415352A1)
    check("PC manager initializer class ID", struct.unpack_from("<I", init, 23)[0], 0xE422E9EB)
    check("PC registration call count", x86_call_count(pc, pc_sections, 0x00022D90), 78)
    check("PC header validator callers", x86_call_count(pc, pc_sections, 0x00022260), 2)
    check("PC scene-loader callers", x86_call_count(pc, pc_sections, 0x00022550), 1)

    pc_object_header_reader = image_slice(pc, pc_sections, 0x00067550, 0x114)
    check(
        "PC object-header reader exact body hash",
        sha256(pc_object_header_reader),
        "9C1EEE14AA544B9E9E8F4550E0F6A233BB28A03764601A40B184DF33393905EF",
    )

    pc_hook_base_init = image_slice(pc, pc_sections, 0x002D5280, 32)
    check(
        "PC spSerializerHook class ID",
        struct.unpack_from("<I", pc_hook_base_init, 20)[0],
        0x18092F8D,
    )
    check(
        "PC spSerializerHook direct base ID",
        struct.unpack_from("<I", pc_hook_base_init, 15)[0],
        0x415352A1,
    )
    pc_dx_hook_init = image_slice(pc, pc_sections, 0x002D4D10, 34)
    check(
        "PC spDXSerializerHook class ID",
        struct.unpack_from("<I", pc_dx_hook_init, 23)[0],
        0x0D832A30,
    )
    check(
        "PC spDXSerializerHook base ID",
        struct.unpack_from("<I", pc_dx_hook_init, 18)[0],
        0x18092F8D,
    )
    check(
        "PC spDXSerializerHook factory remains a SecuROM thunk",
        image_slice(pc, pc_sections, 0x000AA430, 6),
        bytes.fromhex("FF25982D3B01"),
    )

    # PS2 RTTI initializer constructs the same two IDs and exact addresses.
    words = struct.unpack(
        "<14I", image_slice(ps2, ps2_sections, 0x00483090, 14 * 4)
    )
    manager_record = ((words[0] & 0xFFFF) << 16) + signed16(words[6] & 0xFFFF)
    class_id = ((words[1] & 0xFFFF) << 16) | (words[7] & 0xFFFF)
    base_id = ((words[2] & 0xFFFF) << 16) | (words[8] & 0xFFFF)
    class_name = ((words[3] & 0xFFFF) << 16) + signed16(words[9] & 0xFFFF)
    factory = ((words[5] & 0xFFFF) << 16) + signed16(words[11] & 0xFFFF)
    check("PS2 manager registration", manager_record, 0x004A9E50)
    check("PS2 manager class ID", class_id, 0xE422E9EB)
    check("PS2 manager base ID", base_id, 0x415352A1)
    check("PS2 manager class-name address", class_name, 0x00449B70)
    check("PS2 manager factory", factory, 0x00183260)
    manager_vtable = struct.unpack(
        "<7I", image_slice(ps2, ps2_sections, 0x0048F020, 7 * 4)
    )
    check("PS2 manager deleting destructor slot", manager_vtable[2], 0x00182FE0)
    check("PS2 manager clone slot", manager_vtable[4], 0x00183150)
    check("PS2 manager RTTI slot", manager_vtable[6], 0x00181D70)
    check("PS2 registration call count", mips_jal_count(ps2, ps2_sections, 0x00182070), 67)
    registration_tuples = mips_registration_tuples(
        ps2, ps2_sections, 0x00182070
    )
    resolved_tuples = [entry for entry in registration_tuples if None not in entry]
    check("PS2 decoded registration tuple count", len(resolved_tuples), 67)
    tuple_masks = Counter(
        (platform, operation)
        for _, platform, operation in resolved_tuples
    )
    print(
        "INFO PS2 platform/operation registrations: "
        + ", ".join(
            f"0x{platform:X}/0x{operation:X}={count}"
            for (platform, operation), count in sorted(tuple_masks.items())
        )
    )
    check(
        "PS2 platform/operation distribution",
        tuple_masks,
        Counter(
            {
                (0x01, 0x01): 3,
                (0x01, 0x02): 3,
                (0x06, 0x01): 3,
                (0x06, 0x02): 3,
                (0x08, 0x01): 3,
                (0x08, 0x02): 3,
                (0x08, 0x03): 1,
                (0xFF, 0x01): 1,
                (0xFF, 0x02): 1,
                (0xFF, 0x03): 46,
            }
        ),
    )
    check("PS2 FAT-helper constructor callers", mips_jal_count(ps2, ps2_sections, 0x001801C0), 3)
    fat_vtable = struct.unpack(
        "<7I", image_slice(ps2, ps2_sections, 0x0048EF60, 7 * 4)
    )
    check("PS2 FAT-helper deleting destructor slot", fat_vtable[2], 0x0017FDF0)
    check("PS2 FAT-helper inherited RTTI slot", fat_vtable[6], 0x00102830)

    ps2_object_header_reader = image_slice(
        ps2, ps2_sections, 0x001815B0, 0x168
    )
    check(
        "PS2 object-header reader exact body hash",
        sha256(ps2_object_header_reader),
        "470A6AC9D58030EE199197E0FAB735657350D5FEDD1709C558ED80462F7A1158",
    )
    object_header_words = struct.unpack(
        f"<{len(ps2_object_header_reader) // 4}I", ps2_object_header_reader
    )
    marker_load_offsets = [
        word & 0xFFFF
        for word in object_header_words
        if (word >> 26) in (0x23, 0x24)  # LW/LBU
        and ((word >> 21) & 31) == 29    # base SP
        and (word & 0xFFFF) in (0x7C, 0x7D, 0x7E, 0x7F)
    ]
    check("PS2 object-header marker reads after ReadData", marker_load_offsets, [])
    check(
        "PS2 object-header class ID load",
        struct.pack("<I", 0x8FA50078) in ps2_object_header_reader,
        True,
    )

    ps2_hook_base_words = struct.unpack(
        "<13I", image_slice(ps2, ps2_sections, 0x00483010, 13 * 4)
    )
    ps2_hook_base_class_id = (
        ((ps2_hook_base_words[1] & 0xFFFF) << 16)
        | (ps2_hook_base_words[6] & 0xFFFF)
    )
    ps2_hook_registered_base_id = (
        ((ps2_hook_base_words[2] & 0xFFFF) << 16)
        | (ps2_hook_base_words[7] & 0xFFFF)
    )
    check("PS2 spSerializerHook class ID", ps2_hook_base_class_id, 0x18092F8D)
    check(
        "PS2 spSerializerHook direct base ID",
        ps2_hook_registered_base_id,
        0x415352A1,
    )
    ps2_hook_base_vtable = struct.unpack(
        "<10I", image_slice(ps2, ps2_sections, 0x0048EF30, 10 * 4)
    )
    check("PS2 base hook null platform slot", ps2_hook_base_vtable[9], 0)

    ps2_hook_init_words = struct.unpack(
        "<14I", image_slice(ps2, ps2_sections, 0x00485730, 14 * 4)
    )
    ps2_hook_class_id = (
        ((ps2_hook_init_words[1] & 0xFFFF) << 16)
        | (ps2_hook_init_words[7] & 0xFFFF)
    )
    ps2_hook_base_id = (
        ((ps2_hook_init_words[2] & 0xFFFF) << 16)
        | (ps2_hook_init_words[8] & 0xFFFF)
    )
    check("PS2 spPS2SerializerHook class ID", ps2_hook_class_id, 0x1C0E0F30)
    check("PS2 spPS2SerializerHook base ID", ps2_hook_base_id, 0x18092F8D)
    ps2_hook_vtable = struct.unpack(
        "<10I", image_slice(ps2, ps2_sections, 0x00491BB0, 10 * 4)
    )
    check("PS2 hook deleting destructor slot", ps2_hook_vtable[2], 0x00208EE0)
    check("PS2 hook clone slot", ps2_hook_vtable[4], 0x00208F40)
    check("PS2 hook RTTI slot", ps2_hook_vtable[6], 0x00208E60)
    check("PS2 hook platform slot", ps2_hook_vtable[9], 0x00208E70)
    ps2_hook_platform_body = image_slice(ps2, ps2_sections, 0x00208E70, 0x60)
    check(
        "PS2 hook lazily constructs spSerializerManager",
        ps2_hook_platform_body.count(
            struct.pack("<I", mips_jal_word(0x001830E0))
        ),
        1,
    )
    check(
        "PS2 hook platform body",
        struct.unpack(
            "<25I", image_slice(ps2, ps2_sections, 0x00208E70, 25 * 4)
        ),
        (
            0x27BDFFF0, 0xFFBF0000, 0x8F83B86C, 0x50600008, 0x2404002C,
            0x8F84B86C, 0x24030008, 0x8C840010, 0x1483000D, 0x00000000,
            0x1000000C, 0xDFBF0000, 0x0C043614, 0x00000000, 0x0040202D,
            0x10800004, 0x00000000, 0x0C060C38, 0x00000000, 0x0040202D,
            0x1000FFF0, 0xAF84B86C, 0xDFBF0000, 0x03E00008, 0x27BD0010,
        ),
    )

    # These are the exact manager+0x18 loads in three named mesh writer
    # functions. Each is followed by comparisons against 0 and 2; the check
    # prevents the field from regressing to an unobserved/unknown label.
    policy_load_sites = (0x00161DF0, 0x0016255C, 0x00162E60)
    check(
        "PS2 mesh writers consume manager serialization policy +0x18",
        [
            struct.unpack(
                "<I", image_slice(ps2, ps2_sections, address, 4)
            )[0]
            for address in policy_load_sites
        ],
        [0x8C430018] * len(policy_load_sites),
    )

    scene_load = image_slice(ps2, ps2_sections, 0x00182150, 0x4D8)
    scene_load_calls = [
        target
        for target in (0x0017F570, 0x0017F460, 0x0017F8A0, 0x001810F0)
        if struct.pack("<I", mips_jal_word(target)) in scene_load
    ]
    check(
        "PS2 scene-load dependency chain",
        scene_load_calls,
        [0x0017F570, 0x0017F460, 0x0017F8A0, 0x001810F0],
    )
    generic_load = image_slice(ps2, ps2_sections, 0x00182640, 0x36C)
    generic_load_calls = [
        target
        for target in (
            0x0017F570,
            0x0017F460,
            0x00209010,
            0x00182B90,
            0x0017FB70,
            0x0017FA90,
        )
        if struct.pack("<I", mips_jal_word(target)) in generic_load
    ]
    check(
        "PS2 generic-load dependency chain",
        generic_load_calls,
        [
            0x0017F570,
            0x0017F460,
            0x00209010,
            0x00182B90,
            0x0017FB70,
            0x0017FA90,
        ],
    )

    ctor = image_slice(ps2, ps2_sections, 0x001801C0, 0x68)
    ctor_words = struct.unpack(f"<{len(ctor) // 4}I", ctor)
    ctor_calls = [
        0x00102BF0,
        0x001808A0,
        0x00180B70,
        0x00180E40,
        0x0014F2B0,
        0x0014F2B0,
    ]
    actual_ctor_calls = [
        target
        for target in ctor_calls
        if mips_jal_word(target) in ctor_words
    ]
    check("PS2 FAT-helper constructor stages", actual_ctor_calls, ctor_calls)

    load_file_index = image_slice(ps2, ps2_sections, 0x0017F460, 0x110)
    check(
        "PS2 LoadFileIndex reads count and file ID",
        load_file_index.count(struct.pack("<I", mips_jal_word(0x00114E30))),
        2,
    )
    check(
        "PS2 LoadFileIndex reads one filename",
        load_file_index.count(struct.pack("<I", mips_jal_word(0x00114D80))),
        1,
    )

    load_index = image_slice(ps2, ps2_sections, 0x0017F570, 0x2CC)
    check(
        "PS2 LoadIndex reads count and four numeric resource fields",
        load_index.count(struct.pack("<I", mips_jal_word(0x00114E30))),
        5,
    )
    check(
        "PS2 LoadIndex reads one resource name",
        load_index.count(struct.pack("<I", mips_jal_word(0x00114D80))),
        1,
    )
    check(
        "PS2 LoadIndex leaves entry payload-written byte untouched",
        load_index.count(struct.pack("<I", 0xA200001C)),
        0,
    )
    index_object = image_slice(ps2, ps2_sections, 0x0017FC60, 0x188)
    check(
        "PS2 IndexObject initializes resource fileID to zero",
        index_object.count(struct.pack("<I", 0xAE000008)),
        1,
    )
    check(
        "PS2 IndexObject initializes payload-written byte to zero",
        index_object.count(struct.pack("<I", 0xA200001C)),
        1,
    )
    clear_resources = image_slice(ps2, ps2_sections, 0x0017FB70, 0xF0)
    check(
        "PS2 ClearResources resets next resource ID to one",
        struct.pack("<II", 0x24030001, 0xAE030010) in clear_resources,
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
