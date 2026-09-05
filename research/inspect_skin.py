#!/usr/bin/env python3
"""Read-only PC regression check for native spSkin and spSkinSerializer."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from inspect_serializer_manager import PC_SHA256, image_slice, read_pe, sha256


PC_SKIN_VTABLE = (
    0x0046A7A0, 0x005B7A00, 0x0046A1A0, 0x0046A650,
    0x0046A0F0, 0x00408350, 0x00408370, 0x00479B00,
    0x00423FD0, 0x0046A240, 0x004240D0, 0x0046A230,
    0x00479D40, 0x0048EAA0, 0x00479DA0,
)

PC_SERIALIZER_PRIMARY_VTABLE = (
    0x00490D10, 0x005B7A00, 0x00490CC0, 0x0040ECE0,
    0x00490C10, 0x00408350, 0x00408370, 0x00467550,
    0x004671E0, 0x004672C0, 0x00490C40,
)

BODIES = {
    "spSkin registration initializer": (
        0x006D3B20, 0x26,
        "B59CCA3865162A4546A8D4071A2CFF88FF3A7ACB813127DFF66F91B43E653AE4"),
    "spSkin palette setter": (
        0x0046A100, 0x18,
        "B0AF9EC403EEDADC21BA90A665FB7FF7AFC9E0A1994C86689724BB66882C5CC6"),
    "spSkin destructor": (
        0x0046A1F0, 0x31,
        "02162996D7144E67FC71A6C01A343C3B60B7941AF022CDFA3E49780D337E1656"),
    "spSkin copy": (
        0x0046A650, 0x146,
        "57EDD997BD2724BA90B5D06039CC60169E10C6C113E9F18FBEB965D43CC07BE5"),
    "spSkin render": (
        0x0046A240, 0x163,
        "9376D6A7F6E4A5AD2D2AE3E823E61E4B705E8C59D40CC21FC65D5A6403D98F72"),
    "spSkinSerializer registration initializer": (
        0x006D4880, 0x26,
        "9C36B67549535E10C426635AB208B9BABE8B3A4563665C0CE7E28E1852D41DE1"),
    "spSkinSerializer index": (
        0x00490D30, 0x64,
        "269AB2047CDE1CF92DFA9A4A1B58E56BED1261009E6F7B66D39A151A8C6C234D"),
    "spSkinSerializer write": (
        0x00490DA0, 0x3CF,
        "D7EEA1D83E7C74B78338D7C21E9D2907821660CDE6EAA8E446F5762E3C6EFA73"),
    "spSkinSerializer read": (
        0x00491170, 0x365,
        "512276B01DD5D604002BC876BB7D172DDB2E05CF6F7A03A6C276777415846B67"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pc", type=Path,
        default=root / "local-data" / "pc-pristine" / "WinxClub.exe")
    args = parser.parse_args()

    executable = args.pc.read_bytes()
    image_base, sections = read_pe(executable)
    checks = 0
    failures: list[str] = []

    def pc_slice(address: int, size: int) -> bytes:
        return image_slice(executable, sections, address - image_base, size)

    def check(label: str, actual: object, expected: object) -> None:
        nonlocal checks
        checks += 1
        print(f"{'OK' if actual == expected else 'FAIL':4} {label}: {actual!r}")
        if actual != expected:
            failures.append(f"{label}: expected {expected!r}, got {actual!r}")

    check("PC SHA-256", sha256(executable), PC_SHA256)
    for label, (address, size, expected) in BODIES.items():
        check(label, digest(pc_slice(address, size)), expected)

    check("spSkin vtable",
          struct.unpack("<15I", pc_slice(0x006E8C5C, 60)), PC_SKIN_VTABLE)
    check("spSkinSerializer primary vtable",
          struct.unpack("<11I", pc_slice(0x006EC7CC, 44)),
          PC_SERIALIZER_PRIMARY_VTABLE)
    check("spSkinSerializer secondary vtable",
          struct.unpack("<3I", pc_slice(0x006EC7C0, 12)),
          (0x00490DA0, 0x00490D30, 0x00491170))

    setter = pc_slice(0x0046A100, 0x18)
    check("setter writes +0x64/+0x68/+0x6c",
          all(pattern in setter for pattern in (
              bytes.fromhex("89 41 64"), bytes.fromhex("89 51 68"),
              bytes.fromhex("89 41 6C"))), True)
    copy = pc_slice(0x0046A650, 0x146)
    check("copy preserves weight/count/parallel arrays",
          all(pattern in copy for pattern in (
              bytes.fromhex("8B 43 60"), bytes.fromhex("89 45 60"),
              bytes.fromhex("8B 4B 64"), bytes.fromhex("89 4D 64"),
              bytes.fromhex("8B 4B 68"), bytes.fromhex("8B 73 6C"))), True)
    render = pc_slice(0x0046A240, 0x163)
    check("render reads bone palette and inverse binds",
          all(pattern in render for pattern in (
              bytes.fromhex("8B 47 64"), bytes.fromhex("8B 4F 68"),
              bytes.fromhex("8B 4F 6C"))), True)
    check("render publishes and clears renderer bone count",
          bytes.fromhex("BC C9 00 00") in render
          and bytes.fromhex("00 00 00 00") in render[-24:], True)

    writer = pc_slice(0x00490DA0, 0x3CF)
    reader = pc_slice(0x00491170, 0x365)
    check("writer emits weight/count and 64-byte matrices",
          bytes.fromhex("8B 46 60") in writer
          and bytes.fromhex("8B 46 64") in writer
          and bytes.fromhex("6A 40") in writer, True)
    check("reader requests spNode relationships",
          bytes.fromhex("68 65 0F 5C 69") in reader, True)
    check("serializer target is spSkin",
          pc_slice(0x00490C40, 6),
          bytes.fromhex("B8 43 20 1F 68 C3"))
    check("exact serializer source path",
          b"Z:\\Sparkplug\\Code\\Sparkplug\\spSkinSerializer.cpp\x00"
          in executable, True)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} "
          f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
