#!/usr/bin/env python3
"""Locate executable resource-pipeline anchors without modifying the binary."""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class BytePattern:
    name: str
    text: str

    @property
    def tokens(self) -> tuple[int | None, ...]:
        return tuple(None if token == "??" else int(token, 16) for token in self.text.split())


PC_PATTERNS = (
    BytePattern(
        "FindMediaPath",
        "81 EC 38 01 00 00 A1 ?? ?? ?? ?? 55 8B E9 89 84 24 38 01 00 00 "
        "8B 45 14 85 C0 74 ?? 50 E8 ?? ?? ?? ?? 83 C4 04 8D 44 24 08 50 "
        "68 3F 00 0F 00",
    ),
    BytePattern(
        "BuildAssetPath",
        "81 EC 30 01 00 00 A1 ?? ?? ?? ?? 53 55 56 8B E9 8B 4D 14 8D 74 "
        "24 0C 89 84 24 38 01 00 00 57 2B F1 8A 11 88 14 0E 41 84 D2 75 F6",
    ),
    BytePattern(
        "ResourceLoad",
        "56 57 E8 ?? ?? ?? ?? 8B 7C 24 0C 8B F0 8B 06 57 6A 01 8B CE FF "
        "50 20 84 C0 75 ?? 57 E9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 08 5F "
        "33 C0 5E C2 04 00",
    ),
    BytePattern(
        "ValidateFfpsHeader",
        "56 8B 74 24 08 81 3E 46 46 50 53 74 ?? A1 ?? ?? ?? ?? 85 C0 C7 "
        "05 ?? ?? ?? ?? 01 00 01 10 75 ?? E8 ?? ?? ?? ?? A3 ?? ?? ?? ?? "
        "68 74 02 00 00",
    ),
)


D3D9_VTABLE_SLOTS = {
    0x144: "IDirect3DDevice9::DrawPrimitive",
    0x148: "IDirect3DDevice9::DrawIndexedPrimitive",
    0x164: "IDirect3DDevice9::SetFVF",
    0x190: "IDirect3DDevice9::SetStreamSource",
    0x1A0: "IDirect3DDevice9::SetIndices",
}


def find_pattern(data: bytes, pattern: BytePattern) -> list[int]:
    tokens = pattern.tokens
    best_start = 0
    best_length = 0
    current_start = 0
    current_length = 0
    for index in range(len(tokens) + 1):
        if index < len(tokens) and tokens[index] is not None:
            if current_length == 0:
                current_start = index
            current_length += 1
            continue
        if current_length > best_length:
            best_start = current_start
            best_length = current_length
        current_length = 0

    anchor = bytes(value for value in tokens[best_start : best_start + best_length] if value is not None)
    hits: list[int] = []
    search_from = 0
    while True:
        anchor_offset = data.find(anchor, search_from)
        if anchor_offset < 0:
            break
        candidate = anchor_offset - best_start
        if candidate >= 0 and candidate + len(tokens) <= len(data):
            if all(value is None or data[candidate + index] == value for index, value in enumerate(tokens)):
                hits.append(candidate)
        search_from = anchor_offset + 1
    return hits


def direct_x86_references(text: bytes, text_rva: int, target_rva: int) -> list[tuple[int, str]]:
    references: list[tuple[int, str]] = []
    for offset in range(len(text) - 4):
        opcode = text[offset]
        if opcode not in (0xE8, 0xE9):
            continue
        displacement = struct.unpack_from("<i", text, offset + 1)[0]
        source_rva = text_rva + offset
        if source_rva + 5 + displacement == target_rva:
            references.append((source_rva, "CALL" if opcode == 0xE8 else "JMP"))
    return references


def direct_x86_callees(text: bytes, text_rva: int, function_rva: int, size: int) -> list[tuple[int, int]]:
    start = function_rva - text_rva
    if start < 0 or start >= len(text):
        return []
    result: list[tuple[int, int]] = []
    block = text[start : start + size]
    for offset in range(len(block) - 4):
        if block[offset] != 0xE8:
            continue
        source_rva = function_rva + offset
        displacement = struct.unpack_from("<i", block, offset + 1)[0]
        result.append((source_rva, source_rva + 5 + displacement))
    return result


def probable_x86_function_start(text: bytes, text_rva: int, call_offset: int) -> int | None:
    lower = max(0, call_offset - 0x4000)
    marker = text.rfind(b"\xCC\xCC\xCC\xCC", lower, call_offset)
    if marker < 0:
        return None
    while marker < call_offset and text[marker] == 0xCC:
        marker += 1
    return text_rva + marker


def inspect_pe(path: Path, data: bytes) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repo_root / "local-data" / "research-cache" / "python"))
    import pefile  # type: ignore

    pe = pefile.PE(data=data, fast_load=False)
    image_base = pe.OPTIONAL_HEADER.ImageBase
    text_section = next(
        (section for section in pe.sections if section.Name.rstrip(b"\0") == b".text"),
        None,
    )
    if text_section is None:
        raise ValueError("PE does not contain a .text section")
    text = text_section.get_data()
    text_rva = text_section.VirtualAddress

    print("FORMAT PE32-x86")
    print(f"IMAGE_BASE 0x{image_base:08X}")
    resolved: dict[str, int] = {}
    for pattern in PC_PATTERNS:
        file_offsets = find_pattern(data, pattern)
        print(f"ANCHOR {pattern.name} matches={len(file_offsets)}")
        for file_offset in file_offsets:
            rva = pe.get_rva_from_offset(file_offset)
            resolved[pattern.name] = rva
            print(
                f"  file=0x{file_offset:08X} rva=0x{rva:08X} "
                f"va=0x{image_base + rva:08X}"
            )

    for name, target_rva in resolved.items():
        references = direct_x86_references(text, text_rva, target_rva)
        print(f"XREFS {name} count={len(references)}")
        for source_rva, kind in references:
            print(
                f"  {kind} rva=0x{source_rva:08X} "
                f"va=0x{image_base + source_rva:08X}"
            )

    resource_load = resolved.get("ResourceLoad")
    if resource_load is not None:
        callees = direct_x86_callees(text, text_rva, resource_load, 0x60)
        print(f"CALLEES ResourceLoad count={len(callees)}")
        for source_rva, target_rva in callees:
            print(
                f"  CALL source_rva=0x{source_rva:08X} "
                f"target_rva=0x{target_rva:08X} target_va=0x{image_base + target_rva:08X}"
            )

    print("D3D9_ENDPOINTS")
    for displacement, name in D3D9_VTABLE_SLOTS.items():
        encoded = struct.pack("<I", displacement)
        hits: list[int] = []
        for offset in range(len(text) - 5):
            if text[offset] != 0xFF or not 0x90 <= text[offset + 1] <= 0x97:
                continue
            if text[offset + 2 : offset + 6] == encoded:
                hits.append(offset)
        print(f"  {name} matches={len(hits)}")
        for offset in hits:
            call_rva = text_rva + offset
            owner_rva = probable_x86_function_start(text, text_rva, offset)
            owner = "unknown" if owner_rva is None else f"0x{owner_rva:08X}"
            print(
                f"    call_rva=0x{call_rva:08X} va=0x{image_base + call_rva:08X} "
                f"probable_function_rva={owner}"
            )
    return 0


@dataclass(frozen=True)
class ElfSection:
    name: str
    address: int
    offset: int
    size: int
    flags: int


def elf_sections(data: bytes) -> list[ElfSection]:
    section_offset = struct.unpack_from("<I", data, 32)[0]
    entry_size, count, string_index = struct.unpack_from("<HHH", data, 46)
    headers = [
        struct.unpack_from("<IIIIIIIIII", data, section_offset + index * entry_size)
        for index in range(count)
    ]
    names_header = headers[string_index]
    names = data[names_header[4] : names_header[4] + names_header[5]]

    def name_at(offset: int) -> str:
        end = names.find(b"\0", offset)
        return names[offset : len(names) if end < 0 else end].decode("ascii", errors="replace")

    return [
        ElfSection(name_at(header[0]), header[3], header[4], header[5], header[2])
        for header in headers
        if header[5]
    ]


def mips_jal_references(raw: bytes, section_address: int, target: int) -> list[int]:
    encoded = (3 << 26) | ((target >> 2) & 0x03FFFFFF)
    needle = struct.pack("<I", encoded)
    result: list[int] = []
    start = 0
    while True:
        offset = raw.find(needle, start)
        if offset < 0:
            break
        result.append(section_address + offset)
        start = offset + 4
    return result


def probable_mips_function_start(raw: bytes, section_address: int, call_address: int) -> int | None:
    call_offset = call_address - section_address
    for offset in range(call_offset, max(-1, call_offset - 0x4000), -4):
        if offset < 0 or offset + 4 > len(raw):
            continue
        word = struct.unpack_from("<I", raw, offset)[0]
        if word & 0xFFFF0000 == 0x27BD0000 and word & 0x8000:
            return section_address + offset
    return None


def inspect_elf(path: Path, data: bytes) -> int:
    sections = elf_sections(data)
    executable = [section for section in sections if section.flags & 4]
    print("FORMAT ELF32-MIPS-LE")
    print(f"ENTRY 0x{struct.unpack_from('<I', data, 24)[0]:08X}")

    # lui v0,0x5350 followed shortly by ori v0,v0,0x4646.
    high = b"\x50\x53\x02\x3C"
    low = b"\x46\x46\x42\x34"
    validators: list[int] = []
    for section in executable:
        raw = data[section.offset : section.offset + section.size]
        start = 0
        while True:
            offset = raw.find(high, start)
            if offset < 0:
                break
            if raw.find(low, offset + 4, offset + 32) >= 0:
                validators.append(section.address + offset - 4)
            start = offset + 4

    print(f"ANCHOR ValidateFfpsHeader matches={len(validators)}")
    for validator in validators:
        print(f"  va=0x{validator:08X}")
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            callers = mips_jal_references(raw, section.address, validator)
            for caller in callers:
                owner = probable_mips_function_start(raw, section.address, caller)
                owner_text = "unknown" if owner is None else f"0x{owner:08X}"
                print(
                    f"  JAL source_va=0x{caller:08X} "
                    f"probable_function_va={owner_text}"
                )
    print("PS2_RENDER_ENDPOINT open: locate VIF/GIF/GS submission and trace mesh provenance")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    data = args.binary.read_bytes()
    print(f"BINARY {args.binary}")
    print(f"SIZE {len(data)}")
    print(f"SHA256 {hashlib.sha256(data).hexdigest().upper()}")
    if data.startswith(b"MZ"):
        return inspect_pe(args.binary, data)
    if data.startswith(b"\x7FELF") and data[4:6] == b"\x01\x01":
        return inspect_elf(args.binary, data)
    raise ValueError("Expected PE or ELF32 little-endian executable")


if __name__ == "__main__":
    raise SystemExit(main())
