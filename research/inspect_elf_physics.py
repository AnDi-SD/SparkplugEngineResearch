#!/usr/bin/env python3
"""Read-only Sparkplug physics/partition string and xref triage for ELF32 MIPS."""

from __future__ import annotations

import argparse
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


TERMS = re.compile(
    r"collision|collide|physics|physical|partition|sector|portal|bound|"
    r"staticrender|meshbv|raycast|intersect|sphere|capsule|box|bsp|octree|"
    r"spatial|world",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class Section:
    name: str
    address: int
    offset: int
    size: int
    flags: int


def read_sections(data: bytes) -> list[Section]:
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ValueError("Expected ELF32 little-endian input")
    section_offset = struct.unpack_from("<I", data, 32)[0]
    entry_size, count, string_index = struct.unpack_from("<HHH", data, 46)
    headers = [
        struct.unpack_from("<IIIIIIIIII", data, section_offset + index * entry_size)
        for index in range(count)
    ]
    string_header = headers[string_index]
    strings = data[string_header[4] : string_header[4] + string_header[5]]

    def name_at(offset: int) -> str:
        end = strings.find(b"\0", offset)
        if end < 0:
            end = len(strings)
        return strings[offset:end].decode("ascii", errors="replace")

    return [
        Section(name_at(header[0]), header[3], header[4], header[5], header[2])
        for header in headers
        if header[5] > 0
    ]


def ascii_strings(data: bytes, minimum: int = 4):
    pattern = re.compile(rb"[\x20-\x7e]{%d,}" % minimum)
    for match in pattern.finditer(data):
        yield match.start(), match.group().decode("ascii", errors="replace")


def find_section(sections: list[Section], address: int) -> Section | None:
    return next(
        (section for section in sections if section.address <= address < section.address + section.size),
        None,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("--context", action="store_true")
    parser.add_argument("--match", help="Additional case-insensitive regex filter")
    parser.add_argument("--max-xrefs", type=int, default=12)
    parser.add_argument("--disasm-va", action="append", default=[])
    parser.add_argument("--disasm-bytes", type=int, default=768)
    parser.add_argument("--xrefs-va", action="append", default=[])
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repo_root / "local-data" / "research-cache" / "python"))
    import capstone  # type: ignore

    data = args.elf.read_bytes()
    sections = read_sections(data)
    selected = re.compile(args.match, re.IGNORECASE) if args.match else TERMS
    matches: list[tuple[int, str, str]] = []
    for section in sections:
        raw = data[section.offset : section.offset + section.size]
        for offset, value in ascii_strings(raw):
            if selected.search(value):
                matches.append((section.address + offset, section.name, value))

    print(f"ELF={args.elf}")
    print(f"physics_strings={len(matches)}")
    for address, section, value in matches:
        print(f"STRING va=0x{address:08X} section={section}: {value}")

    disassembler = capstone.Cs(
        capstone.CS_ARCH_MIPS,
        capstone.CS_MODE_MIPS32 + capstone.CS_MODE_LITTLE_ENDIAN,
    )
    disassembler.detail = True
    disassembler.skipdata = True
    executable = [section for section in sections if section.flags & 0x4]
    decoded = []
    if args.context or args.xrefs_va:
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            decoded.extend(disassembler.disasm(raw, section.address))
    print(
        f"sections={len(sections)}; executable_sections={len(executable)}; "
        f"decoded_instructions={len(decoded)}"
    )

    for raw_target in args.xrefs_va:
        target = int(raw_target, 0)
        references = []
        for instruction in decoded:
            if instruction.mnemonic not in ("jal", "j") or not instruction.operands:
                continue
            if instruction.operands[0].type == capstone.CS_OP_IMM and instruction.operands[0].imm == target:
                references.append((instruction.address, instruction.mnemonic))
        print(f"CODE_XREFS target_va=0x{target:08X}; count={len(references)}")
        for address, mnemonic in references:
            print(f"  {mnemonic.upper()} source_va=0x{address:08X}")

    for raw_address in args.disasm_va:
        address = int(raw_address, 0)
        section = find_section(sections, address)
        if section is None:
            print(f"No section contains 0x{address:08X}", file=sys.stderr)
            continue
        offset = section.offset + address - section.address
        code = data[offset : offset + args.disasm_bytes]
        print(f"DISASM va=0x{address:08X}; bytes={len(code)}")
        for instruction in disassembler.disasm(code, address):
            print(f"  0x{instruction.address:08X}: {instruction.mnemonic:<8} {instruction.op_str}")

    if not args.context:
        return 0

    by_target: dict[int, list[int]] = {address: [] for address, _, _ in matches}
    for index, instruction in enumerate(decoded):
        if instruction.mnemonic == ".byte":
            continue
        if instruction.mnemonic != "lui" or len(instruction.operands) != 2:
            continue
        destination = instruction.operands[0]
        immediate = instruction.operands[1]
        if destination.type != capstone.CS_OP_REG or immediate.type != capstone.CS_OP_IMM:
            continue
        high = (immediate.imm & 0xFFFF) << 16
        for follower in decoded[index + 1 : index + 7]:
            if follower.mnemonic == ".byte":
                continue
            if len(follower.operands) < 3 or follower.mnemonic not in ("addiu", "ori"):
                continue
            if (
                follower.operands[0].type != capstone.CS_OP_REG
                or follower.operands[1].type != capstone.CS_OP_REG
                or follower.operands[2].type != capstone.CS_OP_IMM
                or follower.operands[1].reg != destination.reg
            ):
                continue
            low = follower.operands[2].imm & 0xFFFF
            if follower.mnemonic == "addiu" and low & 0x8000:
                low -= 0x10000
            target = (high + low) & 0xFFFFFFFF if follower.mnemonic == "addiu" else high | low
            if target in by_target:
                by_target[target].append(instruction.address)
            break

    print("XREFS")
    values = {address: value for address, _, value in matches}
    for target, references in by_target.items():
        if not references:
            continue
        print(f"TARGET va=0x{target:08X}: {values[target]}; xrefs={len(references)}")
        for address in references[: args.max_xrefs]:
            print(f"  XREF va=0x{address:08X}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
