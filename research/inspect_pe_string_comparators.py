#!/usr/bin/env python3
"""List x86 call sites for imported exact and case-folded string comparators."""

from __future__ import annotations

import argparse
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path


COMPARATOR = re.compile(
    r"compare@.*(?:basic_string|char_traits)|stricmp|strcmpi|lstrcmpi|memicmp|strcmp",
    re.IGNORECASE,
)


def load_dependencies(repo_root: Path):
    sys.path.insert(0, str(repo_root / "local-data" / "research-cache" / "python"))
    import capstone  # type: ignore
    import pefile  # type: ignore

    return capstone, pefile


def section_name(section) -> str:
    return section.Name.rstrip(b"\0").decode("ascii", errors="replace")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pe", type=Path)
    parser.add_argument("--context", type=int, default=7)
    parser.add_argument("--match", help="Additional regex applied to import names")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    capstone, pefile = load_dependencies(repo_root)
    pe = pefile.PE(str(args.pe), fast_load=False)
    image_base = pe.OPTIONAL_HEADER.ImageBase
    selected = re.compile(args.match, re.IGNORECASE) if args.match else COMPARATOR

    imports: dict[int, str] = {}
    for descriptor in pe.DIRECTORY_ENTRY_IMPORT:
        module = descriptor.dll.decode("ascii", errors="replace")
        for item in descriptor.imports:
            if not item.name:
                continue
            name = item.name.decode("ascii", errors="replace")
            if selected.search(name):
                imports[item.address] = f"{module}!{name}"

    text = next(
        (section for section in pe.sections if section_name(section) == ".text"),
        None,
    )
    if text is None:
        print("No .text section", file=sys.stderr)
        return 1

    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.skipdata = True
    text_data = text.get_data()
    text_va = image_base + text.VirtualAddress

    # MSVC emits either CALL [IAT] directly or CALL to a JMP [IAT] thunk.
    # Byte scanning avoids decoding the entire multi-megabyte mixed code section.
    thunks: dict[int, int] = {}
    sites: list[tuple[int, int]] = []
    for iat in imports:
        direct = b"\xFF\x15" + struct.pack("<I", iat)
        for match in re.finditer(re.escape(direct), text_data):
            sites.append((text_va + match.start(), iat))
        thunk = b"\xFF\x25" + struct.pack("<I", iat)
        for match in re.finditer(re.escape(thunk), text_data):
            thunks[text_va + match.start()] = iat

    for relative in range(0, len(text_data) - 5):
        if text_data[relative] != 0xE8:
            continue
        displacement = struct.unpack_from("<i", text_data, relative + 1)[0]
        destination = text_va + relative + 5 + displacement
        if destination in thunks:
            sites.append((text_va + relative, thunks[destination]))

    grouped: dict[int, list[int]] = defaultdict(list)
    for address, iat in sites:
        grouped[iat].append(address)

    print(f"PE={args.pe}; image_base=0x{image_base:08X}")
    for iat, name in sorted(imports.items(), key=lambda item: item[1].lower()):
        calls = sorted(set(grouped.get(iat, [])))
        print(
            f"IMPORT {name}; iat_va=0x{iat:08X}; "
            f"call_sites={len(calls)}"
        )
        for call in calls:
            print(f"  CALL rva=0x{call - image_base:08X} va=0x{call:08X}")
            call_relative = call - text_va
            best: tuple[list, int] | None = None
            for start_relative in range(max(0, call_relative - 64), call_relative + 1):
                raw = text_data[start_relative : call_relative + 80]
                decoded = list(decoder.disasm(raw, text_va + start_relative))
                centers = [
                    index for index, instruction in enumerate(decoded)
                    if instruction.address == call
                ]
                if not centers:
                    continue
                candidate = (decoded, centers[0])
                if best is None or candidate[1] > best[1]:
                    best = candidate
            if best is None:
                continue
            decoded, center = best
            start = max(0, center - args.context)
            end = min(len(decoded), center + args.context + 1)
            for instruction in decoded[start:end]:
                marker = "=>" if instruction.address == call else "  "
                print(
                    f"    {marker} 0x{instruction.address - image_base:08X}: "
                    f"{instruction.mnemonic:<7} {instruction.op_str}"
                )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
