#!/usr/bin/env python3
"""Recover original Sparkplug/Winx architecture evidence from game executables.

The scanner is deliberately read-only.  It reports names which are present in the
binary and keeps analytical labels (``engine``, ``game``, ``third_party``) separate
from those original names.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable


# Runtime class registrations consistently use a capital letter after the
# project prefix.  The same strict pattern also keeps unrelated asset strings
# such as ``sparkles`` out of the independent string/registration cross-check.
CLASS_NAME = re.compile(r"^(?:sp|wx)[A-Z][A-Za-z0-9_]{0,125}$")
SOURCE_FILE = re.compile(
    r"(?i)(?:[a-z]:[\\/]|\.?[\\/])[A-Za-z0-9_ .()\\/\-]{1,236}\.(?:cxx|cpp|hpp|cc|c|h)(?![A-Za-z0-9_])"
)


@dataclass(frozen=True)
class SourceEvidence:
    path: str
    boundary: str
    original_module: str | None


@dataclass(frozen=True)
class RegistrationEvidence:
    class_name: str
    class_hash: int
    base_class_hash: int
    base_class_name: str | None
    boundary: str
    registration_rva: int
    registration_object_va: int
    base_registration_va: int
    constructor_arg5_va: int
    constructor_arg6_va: int


@dataclass(frozen=True)
class Ps2RegistrationEvidence:
    class_name: str
    class_hash: int
    base_class_hash: int
    base_class_name: str | None
    boundary: str
    registration_va: int
    registration_object_va: int
    base_registration_va: int
    constructor_t1_va: int
    constructor_t2_value: int


@dataclass(frozen=True)
class ElfSection:
    name: str
    section_type: int
    address: int
    offset: int
    size: int
    flags: int


def ascii_strings(data: bytes, minimum: int = 4) -> Iterable[tuple[int, str]]:
    pattern = re.compile(rb"[\x20-\x7e]{%d,}" % minimum)
    for match in pattern.finditer(data):
        yield match.start(), match.group().decode("ascii", errors="replace")


def classify_name(name: str) -> str:
    if name.startswith("sp"):
        return "engine"
    if name.startswith("wx"):
        return "game"
    return "unknown"


def classify_source(path: str) -> tuple[str, str | None]:
    normalized = path.replace("/", "\\")
    lower = normalized.lower()
    root = "z:\\sparkplug\\code\\"
    if lower.startswith(root):
        relative = normalized[len(root) :]
        original_module = relative.split("\\", 1)[0]
        original_module = {
            "sparkbase": "SparkBase",
            "sparkbasepc": "SparkBasePC",
            "sparkplug": "Sparkplug",
            "sparkplugdx": "SparkplugDX",
            "sparkplugpc": "SparkplugPC",
        }.get(original_module.lower(), original_module)
        if "\\external\\" in lower:
            parts = relative.split("\\")
            external_module = "\\".join(parts[:3]) if len(parts) >= 3 else relative
            return "third_party", external_module
        return "engine", original_module
    if "openssl" in lower or lower.startswith(".\\crypto\\") or lower.startswith(".\\ssl\\"):
        return "third_party", "OpenSSL"
    if "securom" in lower or lower.startswith(".\\src\\") or lower.startswith(".\\obj\\"):
        return "third_party", "SecuROM"
    if "winx" in lower:
        return "game", None
    return "unknown", None


def source_paths(strings: Iterable[str]) -> Iterable[str]:
    for value in strings:
        for match in SOURCE_FILE.finditer(value):
            yield match.group(0)


def push_sequence_before_call(data: bytes, call_offset: int) -> tuple[list[int], int] | None:
    """Decode six immediate pushes and ``mov ecx, imm32`` before a call.

    This matches the observed PC static-registration constructor thunks without
    assigning semantic names to the two still-unidentified callback arguments.
    """
    lower = max(0, call_offset - 48)
    for start in range(lower, call_offset):
        cursor = start
        values: list[int] = []
        for _ in range(6):
            if cursor >= call_offset:
                break
            opcode = data[cursor]
            if opcode == 0x6A and cursor + 2 <= call_offset:
                values.append(data[cursor + 1])
                cursor += 2
            elif opcode == 0x68 and cursor + 5 <= call_offset:
                values.append(struct.unpack_from("<I", data, cursor + 1)[0])
                cursor += 5
            else:
                break
        if len(values) != 6 or cursor + 5 != call_offset or data[cursor] != 0xB9:
            continue
        registration_object = struct.unpack_from("<I", data, cursor + 1)[0]
        return values, registration_object
    return None


def read_c_string_at_va(pe: Any, va: int) -> str | None:
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    if va < image_base:
        return None
    try:
        raw = pe.get_string_at_rva(va - image_base)
    except Exception:
        return None
    if not raw:
        return None
    return raw.decode("ascii", errors="replace")


def read_elf_sections(data: bytes) -> list[ElfSection]:
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
        ElfSection(
            name_at(header[0]), header[1], header[3], header[4], header[5], header[2]
        )
        for header in headers
        if header[5] > 0
    ]


def read_elf_c_string(data: bytes, sections: list[ElfSection], va: int) -> str | None:
    section = next(
        (item for item in sections if item.address <= va < item.address + item.size),
        None,
    )
    if section is None:
        return None
    offset = section.offset + va - section.address
    end = data.find(b"\0", offset, min(len(data), offset + 128))
    if end < 0:
        return None
    return data[offset:end].decode("ascii", errors="replace")


def apply_mips_constant(word: int, constants: dict[int, int]) -> None:
    opcode = word >> 26
    rs = (word >> 21) & 0x1F
    rt = (word >> 16) & 0x1F
    rd = (word >> 11) & 0x1F
    immediate = word & 0xFFFF
    if opcode == 0x0F:  # LUI
        constants[rt] = immediate << 16
        return
    if opcode == 0x0D:  # ORI
        if rs in constants:
            constants[rt] = constants[rs] | immediate
        else:
            constants.pop(rt, None)
        return
    if opcode == 0x09:  # ADDIU
        signed = immediate - 0x10000 if immediate & 0x8000 else immediate
        if rs == 0:
            constants[rt] = signed & 0xFFFFFFFF
        elif rs in constants:
            constants[rt] = (constants[rs] + signed) & 0xFFFFFFFF
        else:
            constants.pop(rt, None)
        return
    if opcode == 0 and (word & 0x3F) in (0x21, 0x25, 0x2D):  # ADDU / OR / DADDU (MOVE)
        if rs == 0 and rt in constants:
            constants[rd] = constants[rt]
        elif rt == 0 and rs in constants:
            constants[rd] = constants[rs]
        else:
            constants.pop(rd, None)
        return
    # Invalidate destinations for the common instructions which may appear in
    # the short thunk window.  Unknown stores/branches do not change a GPR.
    if opcode in (0x08, 0x0A, 0x0B, 0x0C, 0x0E, 0x20, 0x21, 0x23, 0x24, 0x25):
        constants.pop(rt, None)


def scan_ps2_registrations(
    data: bytes, sections: list[ElfSection], constructor_va: int, global_pointer: int
) -> tuple[int, list[Ps2RegistrationEvidence], list[int]]:
    # MIPS register numbers: a0..a3=4..7, t0..t2=8..10.
    raw: list[tuple[int, dict[int, int]]] = []
    call_count = 0
    required = (4, 5, 6, 7, 8, 9, 10)
    for section in sections:
        if not section.flags & 0x4:
            continue
        section_data = data[section.offset : section.offset + section.size]
        for relative in range(0, len(section_data) - 8, 4):
            word = struct.unpack_from("<I", section_data, relative)[0]
            if word >> 26 not in (0x02, 0x03):  # J / JAL
                continue
            call_va = section.address + relative
            target = ((call_va + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
            if target != constructor_va:
                continue
            call_count += 1

            def decode_from(start: int) -> dict[int, int]:
                constants: dict[int, int] = {0: 0, 28: global_pointer}
                for cursor in range(start, relative, 4):
                    apply_mips_constant(
                        struct.unpack_from("<I", section_data, cursor)[0], constants
                    )
                # The delay slot executes before the call.
                apply_mips_constant(
                    struct.unpack_from("<I", section_data, relative + 4)[0], constants
                )
                return constants

            # The common compact thunk is also the safest interpretation near
            # mixed code/data boundaries (notably the root spApp registration).
            short_start = max(0, relative - 96)
            short_start -= short_start % 4
            constants = decode_from(short_start)
            short_name = (
                read_elf_c_string(data, sections, constants[7])
                if all(register in constants for register in required)
                else None
            )
            if short_name is not None and CLASS_NAME.fullmatch(short_name):
                raw.append((call_va, constants))
                continue

            # A few thunks initialize class-owned tables between loading their
            # arguments and making the tail call.  Retry from the preceding tail
            # jump (or return) instead of enlarging the window indiscriminately.
            long_start = max(0, relative - 4096)
            long_start -= long_start % 4
            for predecessor in range(relative - 4, long_start - 1, -4):
                previous = struct.unpack_from("<I", section_data, predecessor)[0]
                previous_opcode = previous >> 26
                previous_function = previous & 0x3F
                previous_target = (
                    ((section.address + predecessor + 4) & 0xF0000000)
                    | ((previous & 0x03FFFFFF) << 2)
                )
                is_tail_jump = previous_opcode == 0x02
                is_return = previous_opcode == 0 and previous_function == 0x08
                is_previous_registration = (
                    previous_opcode == 0x03 and previous_target == constructor_va
                )
                if is_tail_jump or is_return or is_previous_registration:
                    long_start = predecessor + 8  # skip the MIPS delay slot
                    break
            constants = decode_from(long_start)
            raw.append((call_va, constants))

    accepted_raw: list[tuple[int, dict[int, int], str]] = []
    hash_names: dict[int, str] = {}
    for call_va, constants in raw:
        if any(register not in constants for register in required):
            continue
        name = read_elf_c_string(data, sections, constants[7])
        if name is None or not CLASS_NAME.fullmatch(name):
            continue
        hash_names.setdefault(constants[5], name)
        accepted_raw.append((call_va, constants, name))

    registrations = [
        Ps2RegistrationEvidence(
            class_name=name,
            class_hash=constants[5],
            base_class_hash=constants[6],
            base_class_name=hash_names.get(constants[6]),
            boundary=classify_name(name),
            registration_va=call_va,
            registration_object_va=constants[4],
            base_registration_va=constants[8],
            constructor_t1_va=constants[9],
            constructor_t2_value=constants[10],
        )
        for call_va, constants, name in accepted_raw
    ]
    accepted_calls = {call_va for call_va, _, _ in accepted_raw}
    unrecognized = sorted(call_va for call_va, _ in raw if call_va not in accepted_calls)
    return call_count, registrations, unrecognized


def scan_pe(path: Path, registration_constructor_rva: int) -> dict[str, Any]:
    repo_root = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(repo_root / "local-data" / "research-cache" / "python"))
    import pefile  # type: ignore

    data = path.read_bytes()
    pe = pefile.PE(data=data, fast_load=False)
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)

    sources: list[SourceEvidence] = []
    seen_sources: set[str] = set()
    pdb_paths: list[str] = []
    strings = [value for _, value in ascii_strings(data)]
    for value in strings:
        if value.lower().endswith(".pdb") and value not in pdb_paths:
            pdb_paths.append(value)
    for value in source_paths(strings):
        if value.lower() in seen_sources:
            continue
        seen_sources.add(value.lower())
        boundary, module = classify_source(value)
        sources.append(SourceEvidence(value, boundary, module))

    text = next(
        section
        for section in pe.sections
        if section.Name.rstrip(b"\0").decode("ascii", errors="replace") == ".text"
    )
    text_data = text.get_data()
    text_rva = int(text.VirtualAddress)
    registrations_raw: list[tuple[list[int], int, int]] = []
    for relative in range(0, len(text_data) - 5):
        if text_data[relative] != 0xE8:
            continue
        displacement = struct.unpack_from("<i", text_data, relative + 1)[0]
        call_rva = text_rva + relative
        if call_rva + 5 + displacement != registration_constructor_rva:
            continue
        decoded = push_sequence_before_call(text_data, relative)
        if decoded is None:
            continue
        pushes, registration_object = decoded
        registrations_raw.append((pushes, registration_object, call_rva))

    hash_names: dict[int, str] = {}
    accepted: list[tuple[list[int], int, int, str]] = []
    for pushes, registration_object, call_rva in registrations_raw:
        name = read_c_string_at_va(pe, pushes[3])
        if name is None or not CLASS_NAME.fullmatch(name):
            continue
        hash_names.setdefault(pushes[5], name)
        accepted.append((pushes, registration_object, call_rva, name))

    registrations = [
        RegistrationEvidence(
            class_name=name,
            class_hash=pushes[5],
            base_class_hash=pushes[4],
            base_class_name=hash_names.get(pushes[4]),
            boundary=classify_name(name),
            registration_rva=call_rva,
            registration_object_va=registration_object,
            base_registration_va=pushes[2],
            constructor_arg5_va=pushes[1],
            constructor_arg6_va=pushes[0],
        )
        for pushes, registration_object, call_rva, name in accepted
    ]

    unique_by_hash: dict[int, RegistrationEvidence] = {}
    for item in registrations:
        unique_by_hash.setdefault(item.class_hash, item)
    unique = sorted(unique_by_hash.values(), key=lambda item: (item.boundary, item.class_name))
    cross_boundary = [
        item
        for item in unique
        if item.base_class_name is not None
        and item.boundary != classify_name(item.base_class_name)
    ]

    return {
        "path": str(path),
        "format": "PE",
        "sha256": hashlib.sha256(data).hexdigest().upper(),
        "image_base": image_base,
        "pdb_paths": pdb_paths,
        "source_files": [asdict(item) for item in sorted(sources, key=lambda item: item.path.lower())],
        "source_summary": {
            "boundaries": dict(sorted(Counter(item.boundary for item in sources).items())),
            "original_modules": dict(
                sorted(Counter(item.original_module or "(unassigned)" for item in sources).items())
            ),
        },
        "registration_constructor_rva": registration_constructor_rva,
        "registration_calls": len(registrations_raw),
        "recognized_registration_calls": len(registrations),
        "unique_registered_types": len(unique),
        "type_summary": dict(sorted(Counter(item.boundary for item in unique).items())),
        "registered_types": [asdict(item) for item in unique],
        "cross_boundary_inheritance": [asdict(item) for item in cross_boundary],
    }


def scan_elf(path: Path, registration_constructor_va: int) -> dict[str, Any]:
    data = path.read_bytes()
    sections = read_elf_sections(data)
    strings = [value for _, value in ascii_strings(data)]
    class_names = sorted({value for value in strings if CLASS_NAME.fullmatch(value)})
    sources: list[SourceEvidence] = []
    seen: set[str] = set()
    for value in source_paths(strings):
        if value.lower() in seen:
            continue
        seen.add(value.lower())
        boundary, module = classify_source(value)
        sources.append(SourceEvidence(value, boundary, module))
    machine = struct.unpack_from("<H", data, 18)[0] if len(data) >= 20 else None
    entry = struct.unpack_from("<I", data, 24)[0] if len(data) >= 28 else None
    reginfo = next(
        (section for section in sections if section.section_type == 0x70000006),
        None,
    )
    global_pointer = (
        struct.unpack_from("<I", data, reginfo.offset + 20)[0]
        if reginfo is not None and reginfo.size >= 24
        else 0
    )
    registration_calls, registrations, unrecognized_calls = scan_ps2_registrations(
        data, sections, registration_constructor_va, global_pointer
    )
    unique_by_hash: dict[int, Ps2RegistrationEvidence] = {}
    for item in registrations:
        unique_by_hash.setdefault(item.class_hash, item)
    unique = sorted(unique_by_hash.values(), key=lambda item: (item.boundary, item.class_name))
    cross_boundary = [
        item
        for item in unique
        if item.base_class_name is not None
        and item.boundary != classify_name(item.base_class_name)
    ]
    return {
        "path": str(path),
        "format": "ELF",
        "sha256": hashlib.sha256(data).hexdigest().upper(),
        "machine": machine,
        "entry_point": entry,
        "global_pointer": global_pointer,
        "registration_constructor_va": registration_constructor_va,
        "registration_calls": registration_calls,
        "recognized_registration_calls": len(registrations),
        "unrecognized_registration_vas": unrecognized_calls,
        "unique_registered_types": len(unique),
        "registered_types": [asdict(item) for item in unique],
        "cross_boundary_inheritance": [asdict(item) for item in cross_boundary],
        "source_files": [asdict(item) for item in sources],
        "source_summary": {
            "boundaries": dict(sorted(Counter(item.boundary for item in sources).items())),
            "original_modules": dict(
                sorted(Counter(item.original_module or "(unassigned)" for item in sources).items())
            ),
        },
        "class_name_strings": class_names,
        "type_summary": dict(sorted(Counter(item.boundary for item in unique).items())),
        "class_string_summary": dict(
            sorted(Counter(classify_name(name) for name in class_names).items())
        ),
    }


def result_type_names(result: dict[str, Any]) -> set[str]:
    if result.get("registered_types"):
        return {item["class_name"] for item in result["registered_types"]}
    return set(result["class_name_strings"])


def compare_results(left: dict[str, Any], right: dict[str, Any]) -> dict[str, Any]:
    left_names = result_type_names(left)
    right_names = result_type_names(right)
    left_types = {item["class_name"]: item for item in left.get("registered_types", [])}
    right_types = {item["class_name"]: item for item in right.get("registered_types", [])}
    common_registered = set(left_types) & set(right_types)

    def counts(names: set[str]) -> dict[str, int]:
        return dict(sorted(Counter(classify_name(name) for name in names).items()))

    return {
        "left": left["path"],
        "right": right["path"],
        "common": counts(left_names & right_names),
        "left_only": counts(left_names - right_names),
        "right_only": counts(right_names - left_names),
        "left_only_names": sorted(left_names - right_names),
        "right_only_names": sorted(right_names - left_names),
        "common_class_hash_mismatches": sorted(
            name
            for name in common_registered
            if left_types[name]["class_hash"] != right_types[name]["class_hash"]
        ),
        "common_base_hash_mismatches": sorted(
            name
            for name in common_registered
            if left_types[name]["base_class_hash"]
            != right_types[name]["base_class_hash"]
        ),
    }


def compare_class_id_table(result: dict[str, Any], path: Path) -> dict[str, Any]:
    if result["format"] != "PE":
        raise ValueError("A class-ID table can only be checked against decoded PE registrations.")
    native = {item["class_hash"]: item["class_name"] for item in result["registered_types"]}
    rows = [
        (int(raw_hash, 16), name)
        for raw_hash, name in re.findall(
            r"\|\s*`(0x[0-9A-Fa-f]+)`\s*\|\s*`([^`]+)`\s*\|",
            path.read_text(encoding="utf-8"),
        )
    ]
    mismatches = [
        {
            "class_hash": class_hash,
            "table_name": table_name,
            "registered_name": native.get(class_hash),
        }
        for class_hash, table_name in rows
        if native.get(class_hash) != table_name
    ]
    return {
        "path": str(path),
        "rows": len(rows),
        "matching_rows": len(rows) - len(mismatches),
        "mismatches": mismatches,
    }


def compact_report(
    result: dict[str, Any], show_sources: bool, show_types: bool
) -> None:
    print(f"EXECUTABLE {result['path']}")
    print(f"  format={result['format']} sha256={result['sha256']}")
    if "registration_calls" in result:
        print(
            "  registrations="
            f"{result['recognized_registration_calls']}/{result['registration_calls']} "
            f"unique_types={result['unique_registered_types']}"
        )
    print(f"  type_boundaries={json.dumps(result['type_summary'], sort_keys=True)}")
    if result["format"] == "ELF":
        print(
            "  class_string_boundaries="
            f"{json.dumps(result['class_string_summary'], sort_keys=True)}"
        )
    print(
        "  source_boundaries="
        f"{json.dumps(result['source_summary']['boundaries'], sort_keys=True)}"
    )
    print(
        "  original_modules="
        f"{json.dumps(result['source_summary']['original_modules'], sort_keys=True)}"
    )
    if show_sources:
        for item in result["source_files"]:
            print(
                f"  SOURCE boundary={item['boundary']} "
                f"module={item['original_module'] or '-'} path={item['path']}"
            )
    if result.get("registered_types"):
        print(f"  cross_boundary_inheritance={len(result['cross_boundary_inheritance'])}")
        for item in result["cross_boundary_inheritance"]:
            print(
                f"  BOUNDARY {item['class_name']} -> {item['base_class_name']} "
                f"hash=0x{item['class_hash']:08X} base=0x{item['base_class_hash']:08X}"
            )
        if show_types:
            for item in result["registered_types"]:
                base = item["base_class_name"] or f"0x{item['base_class_hash']:08X}"
                print(
                    f"  TYPE {item['boundary']} {item['class_name']} -> {base} "
                    f"hash=0x{item['class_hash']:08X} "
                    f"reg=0x{item.get('registration_rva', item.get('registration_va')):08X}"
                )
        if show_types:
            for address in result.get("unrecognized_registration_vas", []):
                print(f"  UNRECOGNIZED_REGISTRATION va=0x{address:08X}")
    if result["format"] == "ELF" and show_types:
        for name in result["class_name_strings"]:
            if name not in result_type_names(result):
                print(f"  UNREGISTERED_TYPE_STRING {classify_name(name)} {name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executables", nargs="+", type=Path)
    parser.add_argument(
        "--registration-constructor-rva",
        type=lambda value: int(value, 0),
        default=0x12FF0,
        help="PC registration constructor RVA (default: 0x12FF0)",
    )
    parser.add_argument(
        "--ps2-registration-constructor-va",
        type=lambda value: int(value, 0),
        default=0x1115B0,
        help="PS2 registration constructor VA (default: 0x1115B0)",
    )
    parser.add_argument("--json", action="store_true", help="Write machine-readable JSON to stdout")
    parser.add_argument("--show-sources", action="store_true", help="List every recovered source path")
    parser.add_argument("--show-types", action="store_true", help="List every recovered type")
    parser.add_argument(
        "--class-id-table",
        type=Path,
        help="Compare a Markdown `class ID | original name` table with PE registrations",
    )
    args = parser.parse_args()

    results: list[dict[str, Any]] = []
    for path in args.executables:
        magic = path.read_bytes()[:4]
        if magic[:2] == b"MZ":
            results.append(scan_pe(path, args.registration_constructor_rva))
        elif magic == b"\x7fELF":
            results.append(scan_elf(path, args.ps2_registration_constructor_va))
        else:
            raise SystemExit(f"Unsupported executable format: {path}")

    comparisons = [
        compare_results(results[left], results[right])
        for left in range(len(results))
        for right in range(left + 1, len(results))
    ]
    class_table = None
    if args.class_id_table is not None:
        pe_result = next((result for result in results if result["format"] == "PE"), None)
        if pe_result is None:
            raise SystemExit("--class-id-table requires at least one PE executable")
        class_table = compare_class_id_table(pe_result, args.class_id_table)
    if args.json:
        print(
            json.dumps(
                {
                    "executables": results,
                    "comparisons": comparisons,
                    "class_id_table_comparison": class_table,
                },
                ensure_ascii=False,
                indent=2,
            )
        )
    else:
        for index, result in enumerate(results):
            if index:
                print()
            compact_report(result, args.show_sources, args.show_types)
        for comparison in comparisons:
            print()
            print(f"COMPARISON {comparison['left']} <-> {comparison['right']}")
            print(f"  common={json.dumps(comparison['common'], sort_keys=True)}")
            print(f"  left_only={json.dumps(comparison['left_only'], sort_keys=True)}")
            print(f"  right_only={json.dumps(comparison['right_only'], sort_keys=True)}")
            print(
                "  common_identity_mismatches="
                f"class_hash:{len(comparison['common_class_hash_mismatches'])} "
                f"base_hash:{len(comparison['common_base_hash_mismatches'])}"
            )
            if args.show_types:
                for name in comparison["left_only_names"]:
                    print(f"  LEFT_ONLY {classify_name(name)} {name}")
                for name in comparison["right_only_names"]:
                    print(f"  RIGHT_ONLY {classify_name(name)} {name}")
                for name in comparison["common_class_hash_mismatches"]:
                    print(f"  CLASS_HASH_MISMATCH {name}")
                for name in comparison["common_base_hash_mismatches"]:
                    print(f"  BASE_HASH_MISMATCH {name}")
        if class_table is not None:
            print()
            print(f"CLASS_ID_TABLE {class_table['path']}")
            print(
                f"  matching={class_table['matching_rows']}/{class_table['rows']} "
                f"mismatches={len(class_table['mismatches'])}"
            )
            for mismatch in class_table["mismatches"]:
                registered = mismatch["registered_name"] or "(not registered)"
                print(
                    f"  MISMATCH hash=0x{mismatch['class_hash']:08X} "
                    f"table={mismatch['table_name']} registered={registered}"
                )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
