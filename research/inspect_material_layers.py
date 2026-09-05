#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for material pass/texture/std layers."""

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
)


PC_TYPES = (
    # name, init RVA, class, base, name VA, base record, factory, registration
    ("pass", 0x002D37C0, 0x3A8905A5, 0x415352A1,
     0x006E73A4, 0x00755310, 0x0045F610, 0x0075FE80),
    ("texture", 0x002D2900, 0x7F577C6D, 0x415352A1,
     0x006DC93C, 0x00755310, 0x00423470, 0x0075DF70),
    ("standard", 0x002D3850, 0x234C576B, 0x7F577C6D,
     0x006E7720, 0x0075DF70, 0x00460E50, 0x0075FFA8),
)

PS2_TYPES = (
    # name, init VA, class, base, name VA, base record, factory, registration
    ("pass", 0x00482B50, 0x3A8905A5, 0x415352A1,
     0x00447300, 0x0049FF60, 0x00170320, 0x004A9670),
    ("texture", 0x00482B90, 0x7F577C6D, 0x415352A1,
     0x00447320, 0x0049FF60, 0x00170940, 0x004A96D0),
    ("standard", 0x00482BD0, 0x234C576B, 0x7F577C6D,
     0x00447338, 0x004A96D0, 0x00170D30, 0x004A9730),
)

PC_VTABLES = (
    ("pass", 0x002E7388, (
        0x0045F7D0, 0x005B7A00, 0x0045F6A0, 0x0045F740,
        0x0045F560, 0x00408350, 0x00408370)),
    ("texture", 0x002DC91C, (
        0x00423630, 0x005B7A00, 0x004234E0, 0x004235E0,
        0x00423450, 0x00408350, 0x00408370, 0x00423590)),
    ("standard", 0x002E7700, (
        0x00460F00, 0x005B7A00, 0x00460EB0, 0x00460F20,
        0x00460E30, 0x00408350, 0x00408370, 0x00423590)),
)

PS2_VTABLES = (
    ("pass", 0x0048E910, (
        0, 0, 0x00170140, 0x00100810, 0x00170260, 0x0016FF80,
        0x0016FEE0, 0x00100010, 0x00100050)),
    ("texture", 0x0048E940, (
        0, 0, 0x001707A0, 0x00100810, 0x00170870, 0x001706D0,
        0x001706C0, 0x00100010, 0x00100050, 0x00170760)),
    ("standard", 0x0048E970, (
        0, 0, 0x00170B80, 0x00100810, 0x00170C40, 0x00170B20,
        0x00170B10, 0x00100010, 0x00100050, 0x00170760)),
)

PC_BODIES = (
    ("pass initializer", 0x002D37C0, 0x26,
     "C8D4709973DD23E8CA2A8F9B7F8A7624C97667DC1CDA61B9C88E5C4FAF1C0494"),
    ("texture initializer", 0x002D2900, 0x26,
     "02A1FAA70DEAC7133963254A7D1F89FA9B1D7717D38B42CE5A5C46B4E8048B8A"),
    ("standard initializer", 0x002D3850, 0x26,
     "1DFBF8E9850305A003934D48C7DB6EB60A78391866698C91623E989FE855E5A0"),
    ("pass copy", 0x0005F740, 0x88,
     "A1B91D138CE7807DED970E7B001DD272A1BB4988E51AC6F91862E323286323D6"),
    ("texture copy", 0x000235E0, 0x4D,
     "D7EF1DCCA71F8D1BB0D4C8748D56D1EA0ECE737F2421D485F909F0567E18D0C6"),
    ("standard copy", 0x00060F20, 0x35,
     "BB0A6372347EE0C1FD9B72C9C285DA26D388EEFEBE8899FAFEE960BA01AC159A"),
)

PS2_BODIES = (
    ("pass initializer", 0x00482B50, 0x38,
     "8AB643C3B2DA148EFA07C2C2D557B5ADC76A72D5C63D10041A18A92DC5AE0526"),
    ("texture initializer", 0x00482B90, 0x38,
     "C010AA64B0CA1545C742CB0D0FF99867A9B7A5ED4BC582EE006EA0A073EA8D32"),
    ("standard initializer", 0x00482BD0, 0x38,
     "19C030DA9A1A2823B06B38A8C761C2C2B8481D701CA3CA237FA89D46CD5A19B4"),
    ("pass factory", 0x00170320, 0x80,
     "769AFFC4B06BA881D83D7DB1F419E1F8520DF1539FE84D76AC949701EEF1E0B6"),
    ("texture factory", 0x00170940, 0x5C,
     "F9F598F5F5384B0D40BEF5756BC7DFE1A9D8A46EC461F4651BD73144FA46A2F7"),
    ("standard factory", 0x00170D30, 0x7C,
     "375D435018D031FBCF9FED5F0EA495BBB8EC60513019D49B9913246561E0ED1C"),
    ("pass copy", 0x0016FF80, 0x124,
     "F6D815EA6689F04385ABB8F30F00BBB2970E35A959F19307179D6AF664908A78"),
    ("texture copy", 0x001706D0, 0x90,
     "0D519C30FB512C6FCE881FB57C08BBEB9EB93D663EEEFB9A3E1D5E470F53406E"),
    ("standard copy", 0x00170B20, 0x58,
     "0898AD364855F1452E4DA406066BEF8644FB943D794BBAB273034E816F15EA3C"),
)


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def check(label: str, actual: object, expected: object, state: list[int]) -> None:
    state[0] += 1
    ok = actual == expected
    print(f"{'OK  ' if ok else 'FAIL'} {label}: {actual!r}")
    if not ok:
        print(f"     expected: {expected!r}")
        state[1] += 1


def decode_ps2_initializer(raw: bytes) -> tuple[int, ...]:
    words = struct.unpack("<14I", raw)
    address = lambda high, low: ((high & 0xFFFF) << 16) + signed16(low & 0xFFFF)
    raw_id = lambda high, low: ((high & 0xFFFF) << 16) | (low & 0xFFFF)
    return (
        raw_id(words[1], words[7]), raw_id(words[2], words[8]),
        address(words[3], words[9]), address(words[4], words[10]),
        address(words[5], words[11]), address(words[0], words[6]),
    )


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pc", type=Path,
        default=root / "local-data" / "pc-pristine" / "WinxClub.exe")
    parser.add_argument("--ps2", type=Path,
        default=root / "local-data" / "Winx Club the game PS2" / "SLES_532.19")
    args = parser.parse_args()

    pc = args.pc.read_bytes()
    ps2 = args.ps2.read_bytes()
    pc_base, pc_sections = read_pe(pc)
    ps2_sections = read_elf(ps2)
    state = [0, 0]

    check("PC SHA-256", sha256(pc), PC_SHA256, state)
    check("PS2 SHA-256", sha256(ps2), PS2_SHA256, state)
    check("PC image base", pc_base, 0x00400000, state)
    for spelling in (b"spMaterialPassLayer\0", b"spMaterialTextureLayer\0",
                     b"spStdLayer\0"):
        check(f"PC class string {spelling[:-1].decode()}", spelling in pc, True, state)
        check(f"PS2 class string {spelling[:-1].decode()}", spelling in ps2, True, state)

    for name, address, class_id, base_id, string_va, base_record, factory, record in PC_TYPES:
        raw = image_slice(pc, pc_sections, address, 0x26)
        # Immediates are separated by one-byte PUSH opcodes.
        decoded = tuple(struct.unpack_from("<I", raw, offset)[0]
                        for offset in (3, 8, 13, 18, 23, 28))
        check(f"PC {name} initializer fields", decoded,
              (factory, base_record, string_va, base_id, class_id, record), state)

    for name, address, class_id, base_id, string_va, base_record, factory, record in PS2_TYPES:
        decoded = decode_ps2_initializer(image_slice(ps2, ps2_sections, address, 0x38))
        check(f"PS2 {name} initializer fields", decoded,
              (class_id, base_id, string_va, base_record, factory, record), state)

    for name, address, values in PC_VTABLES:
        actual = struct.unpack(f"<{len(values)}I",
            image_slice(pc, pc_sections, address, len(values) * 4))
        check(f"PC {name} vtable", actual, values, state)
    for name, address, values in PS2_VTABLES:
        actual = struct.unpack(f"<{len(values)}I",
            image_slice(ps2, ps2_sections, address, len(values) * 4))
        check(f"PS2 {name} vtable header", actual, values, state)

    for name, address, size, expected in PC_BODIES:
        check(f"PC {name} body hash",
              hashlib.sha256(image_slice(pc, pc_sections, address, size))
              .hexdigest().upper(), expected, state)
    for name, address, size, expected in PS2_BODIES:
        check(f"PS2 {name} body hash",
              hashlib.sha256(image_slice(ps2, ps2_sections, address, size))
              .hexdigest().upper(), expected, state)

    check("PC pass factory is protected thunk",
          image_slice(pc, pc_sections, 0x0005F610, 6),
          bytes.fromhex("FF25281D3B01"), state)
    check("PC texture-layer factory is protected thunk",
          image_slice(pc, pc_sections, 0x00023470, 6),
          bytes.fromhex("FF2500263B01"), state)
    check("PC standard-layer factory is protected thunk",
          image_slice(pc, pc_sections, 0x00060E50, 6),
          bytes.fromhex("FF25B02A3B01"), state)

    pass_factory = image_slice(ps2, ps2_sections, 0x00170320, 0x80)
    texture_factory = image_slice(ps2, ps2_sections, 0x00170940, 0x5C)
    standard_factory = image_slice(ps2, ps2_sections, 0x00170D30, 0x7C)
    check("PS2 pass factory allocates 0x38", bytes.fromhex("38000424") in pass_factory,
          True, state)
    check("PS2 texture-layer factory allocates 0x14",
          bytes.fromhex("14000424") in texture_factory, True, state)
    check("PS2 standard factory allocates outer 0x14 and nested 0x80",
          bytes.fromhex("14000424") in standard_factory
          and bytes.fromhex("80000424") in standard_factory, True, state)
    check("PS2 pass copy uses fixed eight-layer bound",
          bytes.fromhex("0800222e") in image_slice(ps2, ps2_sections, 0x0016FF80, 0x124),
          True, state)
    check("PS2 standard copy delegates texture-layer copy",
          bytes.fromhex("b4c1050c") in image_slice(ps2, ps2_sections, 0x00170B20, 0x58),
          True, state)
    check("PC standard copy delegates texture-layer copy",
          bytes.fromhex("e8b626fcff") in image_slice(pc, pc_sections, 0x00060F20, 0x35),
          True, state)

    print(f"RESULT {'PASS' if state[1] == 0 else 'FAIL'} checks={state[0]-state[1]}/{state[0]}")
    return 0 if state[1] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
