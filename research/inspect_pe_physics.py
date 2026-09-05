#!/usr/bin/env python3
"""Read-only Sparkplug physics/partition string and xref triage for PE files."""

from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path


def load_dependencies(repo_root: Path):
    cache = repo_root / "local-data" / "research-cache" / "python"
    sys.path.insert(0, str(cache))
    import capstone  # type: ignore
    import pefile  # type: ignore

    return capstone, pefile


TERMS = re.compile(
    r"collision|collide|physics|physical|partition|sector|portal|bound|"
    r"staticrender|meshbv|raycast|intersect|sphere|capsule|box|bsp|octree|"
    r"spatial|world",
    re.IGNORECASE,
)


def ascii_strings(data: bytes, minimum: int = 4):
    pattern = re.compile(rb"[\x20-\x7e]{%d,}" % minimum)
    for match in pattern.finditer(data):
        yield match.start(), match.group().decode("ascii", errors="replace")


def section_name(section) -> str:
    return section.Name.rstrip(b"\0").decode("ascii", errors="replace")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pe", type=Path)
    parser.add_argument(
        "--list-sections",
        action="store_true",
        help="Print PE section virtual/raw ranges before other inspection",
    )
    parser.add_argument("--context", action="store_true")
    parser.add_argument("--match", help="Additional case-insensitive regex filter")
    parser.add_argument(
        "--skip-strings",
        action="store_true",
        help="Skip the unrelated ASCII-string scan when only code/data inspection is needed",
    )
    parser.add_argument(
        "--string-rva-start",
        type=lambda value: int(value, 0),
        help="Only print strings whose RVA is at or after this value",
    )
    parser.add_argument(
        "--string-rva-end",
        type=lambda value: int(value, 0),
        help="Only print strings whose RVA is before this value",
    )
    parser.add_argument("--max-xrefs", type=int, default=12)
    parser.add_argument(
        "--disasm-rva",
        action="append",
        default=[],
        help="Disassemble a hexadecimal RVA (repeatable)",
    )
    parser.add_argument(
        "--xrefs-rva",
        action="append",
        default=[],
        help="Find direct x86 CALL/JMP references to a hexadecimal RVA",
    )
    parser.add_argument(
        "--absolute-va-xrefs",
        action="append",
        default=[],
        help="Find raw little-endian references to an absolute 32-bit VA in PE sections",
    )
    parser.add_argument(
        "--memory-offset-xrefs",
        action="append",
        default=[],
        help="Find decoded x86 memory operands using the given displacement",
    )
    parser.add_argument(
        "--code-rva-start",
        type=lambda value: int(value, 0),
        help="Restrict decoded memory xrefs to addresses at or after this RVA",
    )
    parser.add_argument(
        "--code-rva-end",
        type=lambda value: int(value, 0),
        help="Restrict decoded memory xrefs to addresses before this RVA",
    )
    parser.add_argument("--disasm-bytes", type=int, default=768)
    parser.add_argument(
        "--dump-words-rva",
        action="append",
        default=[],
        help="Dump little-endian 32-bit words from a hexadecimal RVA (repeatable)",
    )
    parser.add_argument("--dump-word-count", type=int, default=16)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    capstone, pefile = load_dependencies(repo_root)
    pe = pefile.PE(str(args.pe), fast_load=False)
    image_base = pe.OPTIONAL_HEADER.ImageBase

    if args.list_sections:
        for section in pe.sections:
            print(
                f"SECTION name={section_name(section)!r} "
                f"rva=0x{section.VirtualAddress:08X} "
                f"virtual_size=0x{section.Misc_VirtualSize:X} "
                f"file=0x{section.PointerToRawData:08X} "
                f"raw_size=0x{section.SizeOfRawData:X}"
            )

    matches: list[tuple[int, int, str, str]] = []
    if not args.skip_strings:
        selected = re.compile(args.match, re.IGNORECASE) if args.match else TERMS
        for section in pe.sections:
            raw = section.get_data()
            name = section_name(section)
            for offset, value in ascii_strings(raw):
                rva = section.VirtualAddress + offset
                if args.string_rva_start is not None and rva < args.string_rva_start:
                    continue
                if args.string_rva_end is not None and rva >= args.string_rva_end:
                    continue
                if not selected.search(value):
                    continue
                matches.append((image_base + rva, rva, name, value))

    print(f"PE={args.pe}")
    print(f"image_base=0x{image_base:08X}; physics_strings={len(matches)}")
    for va, rva, section, value in matches:
        print(f"STRING rva=0x{rva:08X} va=0x{va:08X} section={section}: {value}")

    for raw_rva in args.dump_words_rva:
        rva = int(raw_rva, 0)
        try:
            offset = pe.get_offset_from_rva(rva)
        except Exception:
            print(f"No PE data contains RVA 0x{rva:08X}", file=sys.stderr)
            continue
        available = min(args.dump_word_count, max(0, (len(pe.__data__) - offset) // 4))
        print(f"WORDS rva=0x{rva:08X} va=0x{image_base + rva:08X}; count={available}")
        for index in range(available):
            value = struct.unpack_from("<I", pe.__data__, offset + index * 4)[0]
            print(f"  +0x{index * 4:02X} 0x{value:08X}")

    text = next(
        (section for section in pe.sections if section_name(section) == ".text"),
        None,
    )
    if text is None:
        print("No .text section", file=sys.stderr)
        return 1
    text_data = text.get_data()
    text_rva = text.VirtualAddress
    disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    disassembler.detail = True

    if args.memory_offset_xrefs:
        decode_start = max(text_rva, args.code_rva_start or text_rva)
        decode_end = min(
            text_rva + len(text_data),
            args.code_rva_end or text_rva + len(text_data),
        )
        relative_start = decode_start - text_rva
        decoded = disassembler.disasm(
            text_data[relative_start : relative_start + decode_end - decode_start],
            image_base + decode_start,
        )
        requested_displacements = {int(value, 0) for value in args.memory_offset_xrefs}
        references = {displacement: [] for displacement in requested_displacements}
        for instruction in decoded:
            for operand in instruction.operands:
                if operand.type != capstone.CS_OP_MEM:
                    continue
                displacement = operand.mem.disp
                if displacement in references and operand.mem.base:
                    references[displacement].append(
                        (
                            instruction.address - image_base,
                            instruction.mnemonic,
                            instruction.op_str,
                        )
                    )
        for displacement in sorted(references):
            matches_for_offset = references[displacement]
            print(
                f"MEMORY_XREFS displacement={displacement:+#x}; "
                f"count={len(matches_for_offset)}"
            )
            for source_rva, mnemonic, operands in matches_for_offset[: args.max_xrefs]:
                print(
                    f"  source_rva=0x{source_rva:08X}: "
                    f"{mnemonic.upper()} {operands}"
                )

    for raw_target in args.absolute_va_xrefs:
        target_va = int(raw_target, 0)
        needle = struct.pack("<I", target_va)
        references = []
        for section in pe.sections:
            raw = section.get_data()
            references.extend(
                (
                    section.VirtualAddress + match.start(),
                    section_name(section),
                )
                for match in re.finditer(re.escape(needle), raw)
            )
        print(f"ABSOLUTE_XREFS target_va=0x{target_va:08X}; count={len(references)}")
        for source_rva, source_section in references[: args.max_xrefs]:
            print(f"  RAW source_rva=0x{source_rva:08X} section={source_section}")

    for raw_target in args.xrefs_rva:
        target_rva = int(raw_target, 0)
        references: list[tuple[int, str]] = []
        for relative in range(0, len(text_data) - 5):
            opcode = text_data[relative]
            if opcode not in (0xE8, 0xE9):
                continue
            displacement = struct.unpack_from("<i", text_data, relative + 1)[0]
            source_rva = text_rva + relative
            destination_rva = source_rva + 5 + displacement
            if destination_rva == target_rva:
                references.append((source_rva, "CALL" if opcode == 0xE8 else "JMP"))
        print(
            f"CODE_XREFS target_rva=0x{target_rva:08X}; "
            f"count={len(references)}"
        )
        for source_rva, kind in references[: args.max_xrefs]:
            print(f"  {kind} source_rva=0x{source_rva:08X}")

    for raw_rva in args.disasm_rva:
        rva = int(raw_rva, 0)
        code = pe.get_data(rva, args.disasm_bytes)
        print(f"DISASM rva=0x{rva:08X}; bytes={len(code)}")
        padding = 0
        for instruction in disassembler.disasm(code, image_base + rva):
            print(
                f"  0x{instruction.address - image_base:08X}: "
                f"{instruction.mnemonic:<7} {instruction.op_str}"
            )
            if instruction.mnemonic == "int3":
                padding += 1
                if padding >= 8:
                    break
            else:
                padding = 0

    if not args.context:
        return 0

    print("XREFS")
    for va, rva, _, value in matches:
        needle = struct.pack("<I", va)
        refs = [match.start() for match in re.finditer(re.escape(needle), text_data)]
        if not refs:
            continue
        print(f"TARGET rva=0x{rva:08X}: {value}; text_xrefs={len(refs)}")
        for relative in refs[: args.max_xrefs]:
            ref_rva = text_rva + relative
            print(f"  XREF rva=0x{ref_rva:08X}")
            start = max(0, relative - 24)
            end = min(len(text_data), relative + 40)
            code = text_data[start:end]
            for instruction in disassembler.disasm(code, image_base + text_rva + start):
                marker = "=>" if instruction.address <= image_base + ref_rva < instruction.address + instruction.size else "  "
                print(
                    f"    {marker} 0x{instruction.address - image_base:08X}: "
                    f"{instruction.mnemonic:<7} {instruction.op_str}"
                )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
