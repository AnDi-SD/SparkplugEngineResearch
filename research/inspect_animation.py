#!/usr/bin/env python3
"""Read-only PC regression check for native spAnimation and its serializer."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from inspect_serializer_manager import PC_SHA256, image_slice, read_pe, sha256


ANIMATION_VTABLE = (
    0x00430380, 0x005B7A00, 0x0041AA70, 0x00413120,
    0x00430280, 0x00408350, 0x00408370,
)

CONTROLLER_VTABLE = (
    0x004230E0, 0x005B7A00, 0x004A1BF0, 0x00423100,
    0x00423070, 0x00408350, 0x00408370, 0x0060DB76,
)

SUB_CONTROLLER_VTABLE = (
    0x00467970, 0x005B7A00, 0x004A1BF0, 0x0040ECE0,
    0x00467950, 0x00408350, 0x00408370, 0x0060DB76,
)

SERIALIZER_STREAM_VTABLE = (0x0043DFE0, 0x005A7DB0, 0x0043ECC0)

SERIALIZER_PRIMARY_VTABLE = (
    0x0043DB70, 0x005B7A00, 0x0043DB20, 0x0040ECE0,
    0x0043DA20, 0x00408350, 0x00408370, 0x00467550,
    0x004671E0, 0x004672C0, 0x0043DA50,
)

BODIES = {
    "spAnimation clone": (0x0041AA70, 0x49,
        "42D861EBED2BA3C7C8B397FF8A88555EBB0C4CEE229AFBE4344D6290F667BF4F"),
    "spAnimation track resize": (0x00430010, 0xC3,
        "D7E686958592CC7E4470F10B6C5F38D614D2B2530A5A419D21793D8B59816B8F"),
    "spAnimation destructor": (0x00430130, 0x14B,
        "76DDF0A536F8B56FDC25A88A2CD7BEE270CB4A9FB4936731A66820747B6D167C"),
    "spAnimation registration getter": (0x00430280, 0x6,
        "971A23F3C14271490F964FAE25293AA9D23A734A0EC4463AB8CF782F126569F7"),
    "spAnimation tag insertion": (0x004305F0, 0x53,
        "1B13EBDAA9DA3400C72366A986FD493DD786AFC8B8F3C7A8074BC8DCBC2D2B93"),
    "serializer registration getter": (0x0043DA20, 0x6,
        "92C14C01C9A20B8E7961445F7993AC71B9CF92AF764969693EE94C4976E618A4"),
    "serializer target": (0x0043DA50, 0x6,
        "E80DBF70A39EA384287870A2C6C2935C82F80A34421EF19DF203F472FC8DEE7A"),
    "serializer writer": (0x0043DFE0, 0xCD5,
        "323163548DD2BA23C00E0A378695C8F631D0AC83F2EAF4A30B0408F8DE5B1D9E"),
    "serializer reader": (0x0043ECC0, 0x6DC,
        "E6288678FDDB2996FE94A6B903F542B2A2137BE8057302334538FE84E9DD61C6"),
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

    check("spAnimation vtable",
          struct.unpack("<7I", pc_slice(0x006DE6CC, 28)), ANIMATION_VTABLE)
    check("spController vtable",
          struct.unpack("<8I", pc_slice(0x006DC86C, 32)), CONTROLLER_VTABLE)
    check("spSubController vtable",
          struct.unpack("<8I", pc_slice(0x006E83E4, 32)), SUB_CONTROLLER_VTABLE)
    check("spController registration getter",
          pc_slice(0x00423070, 6), bytes.fromhex("B8 50 DE 75 00 C3"))
    check("spSubController registration getter",
          pc_slice(0x00467950, 6), bytes.fromhex("B8 40 03 76 00 C3"))
    check("spAnimationSerializer stream vtable",
          struct.unpack("<3I", pc_slice(0x006E0B00, 12)),
          SERIALIZER_STREAM_VTABLE)
    check("spAnimationSerializer primary vtable",
          struct.unpack("<11I", pc_slice(0x006E0B0C, 44)),
          SERIALIZER_PRIMARY_VTABLE)

    destructor = pc_slice(0x00430130, 0x14B)
    check("track stride is 0x44", bytes.fromhex("83 C7 44") in destructor, True)
    check("track array/count fields are +0x1c/+0x20",
          bytes.fromhex("8B 46 1C") in destructor
          and bytes.fromhex("8B 46 20") in destructor, True)
    check("tag vector uses +0x2c/+0x30/+0x34",
          all(pattern in destructor for pattern in (
              bytes.fromhex("8B 46 2C"), bytes.fromhex("8B 46 30"),
              bytes.fromhex("89 6E 34"))), True)
    check("six owned auxiliary arrays begin at +0x38",
          bytes.fromhex("8D 7E 38") in destructor
          and bytes.fromhex("BB 06 00 00 00") in destructor, True)
    check("spAnimation destructor returns through spBaseObject body",
          bytes.fromhex("E8 28 2E FE FF") in destructor, True)

    resize = pc_slice(0x00430010, 0xC3)
    check("resize allocates count times 0x44",
          bytes.fromhex("6B C0 44") in resize, True)
    insertion = pc_slice(0x004305F0, 0x53)
    check("tags are ordered by float at +0x14",
          bytes.fromhex("D9 43 14") in insertion
          and bytes.fromhex("D8 58 14") in insertion, True)
    reader = pc_slice(0x0043ECC0, 0x6DC)
    check("reader dispatches extended field 64",
          bytes.fromhex("83 F9 40") in reader, True)
    check("serializer target is spAnimation",
          pc_slice(0x0043DA50, 6), bytes.fromhex("B8 3A 56 EE 56 C3"))
    check("exact serializer source path",
          b"Z:\\Sparkplug\\Code\\Sparkplug\\spAnimationSerializer.cpp\x00"
          in executable, True)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} "
          f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
