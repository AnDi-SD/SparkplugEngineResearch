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
    section_type: int
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
        Section(name_at(header[0]), header[1], header[3], header[4], header[5], header[2])
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
    parser.add_argument("--list-sections", action="store_true")
    parser.add_argument("--context", action="store_true")
    parser.add_argument("--match", help="Additional case-insensitive regex filter")
    parser.add_argument(
        "--skip-strings",
        action="store_true",
        help="Skip the unrelated ASCII-string scan when only code/data inspection is needed",
    )
    parser.add_argument(
        "--string-va-start",
        type=lambda value: int(value, 0),
        help="Only print strings whose VA is at or after this value",
    )
    parser.add_argument(
        "--string-va-end",
        type=lambda value: int(value, 0),
        help="Only print strings whose VA is before this value",
    )
    parser.add_argument("--max-xrefs", type=int, default=12)
    parser.add_argument("--disasm-va", action="append", default=[])
    parser.add_argument("--disasm-bytes", type=int, default=768)
    parser.add_argument("--xrefs-va", action="append", default=[])
    parser.add_argument(
        "--absolute-va-xrefs",
        action="append",
        default=[],
        help="Find raw little-endian references to an absolute 32-bit VA in ELF sections",
    )
    parser.add_argument(
        "--constructed-va-xrefs",
        action="append",
        default=[],
        help="Find MIPS LUI plus ADDIU/ORI constructions of a 32-bit VA",
    )
    parser.add_argument(
        "--gp-offset-xrefs",
        action="append",
        default=[],
        help="Find direct MIPS loads/stores using a signed GP-relative offset",
    )
    parser.add_argument(
        "--memory-offset-xrefs",
        action="append",
        default=[],
        help="Find MIPS loads/stores using a signed base-register offset",
    )
    parser.add_argument(
        "--immediate-xrefs",
        action="append",
        default=[],
        help=(
            "Find MIPS I-type instructions whose low 16-bit immediate matches "
            "the requested integer (triage aid; may include false positives)"
        ),
    )
    parser.add_argument(
        "--same-base-offsets",
        action="append",
        default=[],
        help=(
            "Find nearby MIPS memory operations using one base register and all "
            "comma-separated offsets; the final offset is the anchor"
        ),
    )
    parser.add_argument(
        "--same-base-window",
        type=int,
        default=24,
        help="Instruction radius used by --same-base-offsets (default: 24)",
    )
    parser.add_argument(
        "--code-va-start",
        type=lambda value: int(value, 0),
        help="Restrict raw code xref scans to addresses at or after this VA",
    )
    parser.add_argument(
        "--code-va-end",
        type=lambda value: int(value, 0),
        help="Restrict raw code xref scans to addresses before this VA",
    )
    parser.add_argument(
        "--member-call-xrefs",
        action="store_true",
        help=(
            "Find calls through the PS2 12-byte member-function descriptor "
            "helpers at 0x3FE4C0/0x3FE500"
        ),
    )
    parser.add_argument(
        "--dump-words-va",
        action="append",
        default=[],
        help="Dump little-endian 32-bit words from a hexadecimal VA (repeatable)",
    )
    parser.add_argument("--dump-word-count", type=int, default=16)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    if str(args.elf) == "@ps2":
        args.elf = (
            repo_root
            / "local-data"
            / "Winx Club the game PS2"
            / "SLES_532.19"
        )
    sys.path.insert(0, str(repo_root / "local-data" / "research-cache" / "python"))
    import capstone  # type: ignore

    data = args.elf.read_bytes()
    sections = read_sections(data)
    if args.list_sections:
        print(f"ELF={args.elf}")
        for section in sections:
            suffix = ""
            if section.section_type == 0x70000006 and section.size >= 24:
                gp_value = struct.unpack_from("<I", data, section.offset + 20)[0]
                suffix = f" gp=0x{gp_value:08X}"
            print(
                f"SECTION name={section.name!r} type=0x{section.section_type:08X} "
                f"va=0x{section.address:08X} file=0x{section.offset:08X} "
                f"size=0x{section.size:X} flags=0x{section.flags:X}{suffix}"
            )
    matches: list[tuple[int, str, str]] = []
    if not args.skip_strings:
        selected = re.compile(args.match, re.IGNORECASE) if args.match else TERMS
        for section in sections:
            raw = data[section.offset : section.offset + section.size]
            for offset, value in ascii_strings(raw):
                address = section.address + offset
                if args.string_va_start is not None and address < args.string_va_start:
                    continue
                if args.string_va_end is not None and address >= args.string_va_end:
                    continue
                if selected.search(value):
                    matches.append((address, section.name, value))

    print(f"ELF={args.elf}")
    print(f"physics_strings={len(matches)}")
    for address, section, value in matches:
        print(f"STRING va=0x{address:08X} section={section}: {value}")

    disassembler = capstone.Cs(
        capstone.CS_ARCH_MIPS,
        # The ELF container is 32-bit, but the PS2 Emotion Engine uses the
        # R5900's 64-bit GPR instructions (sd/ld/daddu) throughout ordinary
        # C++ code.  MIPS32 mode turned those into misleading skip-data bytes.
        capstone.CS_MODE_MIPS64 + capstone.CS_MODE_LITTLE_ENDIAN,
    )
    disassembler.detail = True
    disassembler.skipdata = True
    executable = [section for section in sections if section.flags & 0x4]
    decoded = []
    # Full Capstone decoding is only needed for string-context analysis.
    # Direct J/JAL xrefs below are scanned as raw fixed-width instructions;
    # this keeps function triage cheap even for the complete PS2 text section.
    full_decode_requested = args.context
    if full_decode_requested:
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            decoded.extend(disassembler.disasm(raw, section.address))
    decoded_status = str(len(decoded)) if full_decode_requested else "not_requested"
    print(
        f"sections={len(sections)}; executable_sections={len(executable)}; "
        f"decoded_instructions={decoded_status}"
    )

    for raw_address in args.dump_words_va:
        address = int(raw_address, 0)
        section = find_section(sections, address)
        if section is None:
            print(f"No section contains 0x{address:08X}", file=sys.stderr)
            continue
        offset = section.offset + address - section.address
        available = min(args.dump_word_count, (section.size - (address - section.address)) // 4)
        print(f"WORDS va=0x{address:08X}; count={available}")
        for index in range(available):
            value = struct.unpack_from("<I", data, offset + index * 4)[0]
            print(f"  +0x{index * 4:02X} 0x{value:08X}")

    for raw_target in args.xrefs_va:
        target = int(raw_target, 0)
        references = []
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            for offset in range(0, len(raw) - 3, 4):
                word = struct.unpack_from("<I", raw, offset)[0]
                opcode = word >> 26
                if opcode not in (2, 3):
                    continue
                address = section.address + offset
                destination = ((address + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
                if destination == target:
                    references.append((address, "j" if opcode == 2 else "jal"))
        print(f"CODE_XREFS target_va=0x{target:08X}; count={len(references)}")
        for address, mnemonic in references[: args.max_xrefs]:
            print(f"  {mnemonic.upper()} source_va=0x{address:08X}")

    for raw_target in args.absolute_va_xrefs:
        target = int(raw_target, 0)
        needle = struct.pack("<I", target)
        references = []
        for section in sections:
            raw = data[section.offset : section.offset + section.size]
            references.extend(
                (section.address + match.start(), section.name)
                for match in re.finditer(re.escape(needle), raw)
            )
        print(f"ABSOLUTE_XREFS target_va=0x{target:08X}; count={len(references)}")
        for address, source_section in references[: args.max_xrefs]:
            print(f"  RAW source_va=0x{address:08X} section={source_section}")

    for raw_target in args.constructed_va_xrefs:
        target = int(raw_target, 0)
        references = []
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            words = [
                struct.unpack_from("<I", raw, offset)[0]
                for offset in range(0, len(raw) - 3, 4)
            ]
            for index, word in enumerate(words):
                if word >> 26 != 0x0F:  # LUI
                    continue
                destination_register = (word >> 16) & 0x1F
                high = (word & 0xFFFF) << 16
                for follower_index in range(index + 1, min(index + 7, len(words))):
                    follower = words[follower_index]
                    opcode = follower >> 26
                    if opcode not in (0x09, 0x0D):  # ADDIU / ORI
                        continue
                    source_register = (follower >> 21) & 0x1F
                    if source_register != destination_register:
                        continue
                    low = follower & 0xFFFF
                    if opcode == 0x09 and low & 0x8000:
                        low -= 0x10000
                    value = (high + low) & 0xFFFFFFFF if opcode == 0x09 else high | low
                    if value == target:
                        references.append(
                            (
                                section.address + index * 4,
                                section.address + follower_index * 4,
                                "ADDIU" if opcode == 0x09 else "ORI",
                            )
                        )
                    break
        print(f"CONSTRUCTED_XREFS target_va=0x{target:08X}; count={len(references)}")
        for lui_address, follower_address, mnemonic in references[: args.max_xrefs]:
            print(
                f"  LUI source_va=0x{lui_address:08X}; "
                f"{mnemonic} source_va=0x{follower_address:08X}"
            )

    memory_opcodes = {
        0x20: "lb",
        0x21: "lh",
        0x23: "lw",
        0x24: "lbu",
        0x25: "lhu",
        0x28: "sb",
        0x29: "sh",
        0x2B: "sw",
        0x37: "ld",
        0x3F: "sd",
    }

    def address_is_selected(address: int) -> bool:
        return (
            (args.code_va_start is None or address >= args.code_va_start)
            and (args.code_va_end is None or address < args.code_va_end)
        )

    for raw_immediate in args.immediate_xrefs:
        requested_immediate = int(raw_immediate, 0)
        encoded_immediate = requested_immediate & 0xFFFF
        references = []
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            for offset in range(0, len(raw) - 3, 4):
                word = struct.unpack_from("<I", raw, offset)[0]
                opcode = word >> 26
                address = section.address + offset
                if (
                    opcode not in (0, 2, 3)
                    and (word & 0xFFFF) == encoded_immediate
                    and address_is_selected(address)
                ):
                    references.append((address, word, opcode))
        print(
            f"IMMEDIATE_XREFS value={requested_immediate:+#x}; "
            f"encoded=0x{encoded_immediate:04X}; count={len(references)}"
        )
        for address, word, opcode in references[: args.max_xrefs]:
            print(
                f"  source_va=0x{address:08X} word=0x{word:08X} opcode=0x{opcode:02X}"
            )

    for raw_offset in args.gp_offset_xrefs:
        target_offset = int(raw_offset, 0)
        references = []
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            for offset in range(0, len(raw) - 3, 4):
                word = struct.unpack_from("<I", raw, offset)[0]
                opcode = word >> 26
                base_register = (word >> 21) & 0x1F
                immediate = word & 0xFFFF
                if immediate & 0x8000:
                    immediate -= 0x10000
                if (
                    opcode in memory_opcodes
                    and base_register == 28
                    and immediate == target_offset
                    and address_is_selected(section.address + offset)
                ):
                    references.append(
                        (section.address + offset, memory_opcodes[opcode], (word >> 16) & 0x1F)
                    )
        print(f"GP_XREFS offset={target_offset:+#x}; count={len(references)}")
        for address, mnemonic, register in references[: args.max_xrefs]:
            print(
                f"  {mnemonic.upper()} source_va=0x{address:08X} "
                f"register=${register}"
            )

    for raw_offset in args.memory_offset_xrefs:
        target_offset = int(raw_offset, 0)
        references = []
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            for offset in range(0, len(raw) - 3, 4):
                word = struct.unpack_from("<I", raw, offset)[0]
                opcode = word >> 26
                base_register = (word >> 21) & 0x1F
                immediate = word & 0xFFFF
                if immediate & 0x8000:
                    immediate -= 0x10000
                address = section.address + offset
                if (
                    opcode in memory_opcodes
                    and immediate == target_offset
                    and address_is_selected(address)
                ):
                    references.append(
                        (
                            address,
                            memory_opcodes[opcode],
                            (word >> 16) & 0x1F,
                            base_register,
                        )
                    )
        print(f"MEMORY_XREFS offset={target_offset:+#x}; count={len(references)}")
        for address, mnemonic, register, base_register in references[: args.max_xrefs]:
            print(
                f"  {mnemonic.upper()} source_va=0x{address:08X} "
                f"register=${register} base=${base_register}"
            )

    for raw_offsets in args.same_base_offsets:
        requested_offsets = tuple(int(value.strip(), 0) for value in raw_offsets.split(","))
        if len(requested_offsets) < 2:
            parser.error("--same-base-offsets requires at least two comma-separated offsets")
        anchor_offset = requested_offsets[-1]
        groups = []
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            accesses = []
            for offset in range(0, len(raw) - 3, 4):
                word = struct.unpack_from("<I", raw, offset)[0]
                opcode = word >> 26
                if opcode not in memory_opcodes:
                    continue
                immediate = word & 0xFFFF
                if immediate & 0x8000:
                    immediate -= 0x10000
                address = section.address + offset
                if not address_is_selected(address):
                    continue
                accesses.append(
                    (
                        offset // 4,
                        address,
                        memory_opcodes[opcode],
                        (word >> 16) & 0x1F,
                        (word >> 21) & 0x1F,
                        immediate,
                    )
                )
            accesses_by_index = {access[0]: access for access in accesses}
            for anchor in accesses:
                instruction_index, address, _, _, base_register, immediate = anchor
                if immediate != anchor_offset:
                    continue
                nearby = [
                    access
                    for candidate_index in range(
                        instruction_index - args.same_base_window,
                        instruction_index + args.same_base_window + 1,
                    )
                    if (access := accesses_by_index.get(candidate_index)) is not None
                    and access[4] == base_register
                ]
                observed_offsets = {access[5] for access in nearby}
                if all(offset in observed_offsets for offset in requested_offsets):
                    groups.append((address, base_register, nearby))
        print(
            f"SAME_BASE_OFFSETS offsets={','.join(f'{value:+#x}' for value in requested_offsets)}; "
            f"window={args.same_base_window}; count={len(groups)}"
        )
        for anchor_address, base_register, nearby in groups[: args.max_xrefs]:
            summary = ", ".join(
                f"0x{address:08X}:{mnemonic.upper()}({offset:+#x})"
                for _, address, mnemonic, _, _, offset in nearby
                if offset in requested_offsets
            )
            print(
                f"  anchor_va=0x{anchor_address:08X} base=${base_register}: {summary}"
            )

    if args.member_call_xrefs:
        helper_targets = {0x003FE4C0: "object_a0", 0x003FE500: "object_a1"}
        references = []
        for section in executable:
            raw = data[section.offset : section.offset + section.size]
            for offset in range(0, len(raw) - 7, 4):
                word = struct.unpack_from("<I", raw, offset)[0]
                if word >> 26 != 3:
                    continue
                address = section.address + offset
                target = ((address + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
                if target not in helper_targets or not address_is_selected(address):
                    continue
                delay = struct.unpack_from("<I", raw, offset + 4)[0]
                opcode = delay >> 26
                source_register = (delay >> 21) & 0x1F
                destination_register = (delay >> 16) & 0x1F
                immediate = delay & 0xFFFF
                if immediate & 0x8000:
                    immediate -= 0x10000
                if opcode == 9 and destination_register == 25:
                    references.append(
                        (address, target, source_register, immediate, helper_targets[target])
                    )
        print(f"MEMBER_CALL_XREFS count={len(references)}")
        for address, target, base_register, descriptor_offset, object_register in references[
            : args.max_xrefs
        ]:
            print(
                f"  JAL source_va=0x{address:08X} helper=0x{target:08X} "
                f"descriptor=${base_register}{descriptor_offset:+#x} "
                f"adjusts={object_register}"
            )

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
