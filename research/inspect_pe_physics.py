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
    parser.add_argument("--context", action="store_true")
    parser.add_argument("--match", help="Additional case-insensitive regex filter")
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
    parser.add_argument("--disasm-bytes", type=int, default=768)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    capstone, pefile = load_dependencies(repo_root)
    pe = pefile.PE(str(args.pe), fast_load=False)
    image_base = pe.OPTIONAL_HEADER.ImageBase

    selected = re.compile(args.match, re.IGNORECASE) if args.match else TERMS
    matches: list[tuple[int, int, str, str]] = []
    for section in pe.sections:
        raw = section.get_data()
        name = section_name(section)
        for offset, value in ascii_strings(raw):
            if not selected.search(value):
                continue
            rva = section.VirtualAddress + offset
            matches.append((image_base + rva, rva, name, value))

    print(f"PE={args.pe}")
    print(f"image_base=0x{image_base:08X}; physics_strings={len(matches)}")
    for va, rva, section, value in matches:
        print(f"STRING rva=0x{rva:08X} va=0x{va:08X} section={section}: {value}")

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
        for source_rva, kind in references:
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
