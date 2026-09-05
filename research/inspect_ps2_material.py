#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for the native spPS2Material leaf."""

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


PS2_BODIES = {
    "registration initializer": (
        0x00485210, 0x38,
        "C68E3791E35FEB63CF2E117E2168A69985D60A727229CFB8FC3625569EEE137D"),
    "registration getter": (
        0x001F20A0, 0x0C,
        "98C309B0AC1359D998ECA6102F33AB96234431098BCB34D80D30E283B7508B2A"),
    "copy": (
        0x001F20B0, 0x104,
        "2318E6918DC0B1A4A379B88A46DEB7DB9DAB529465303B9A4F93F6D962511FB7"),
    "specular-power getter": (
        0x001F21C0, 0x08,
        "841FB77F355694E0A2554E5CEAB03E4C24EAA140FF99BFD940F09EF6765A7875"),
    "specular-power setter": (
        0x001F21D0, 0x08,
        "F01E7BAE3A653F98DBB0E5AE684E8B5D423AE4731D258D56F4104C92AAABAFE7"),
    "pass update": (
        0x001F2360, 0xC4,
        "43F7E3BCC79017AB7556F3EFC89E0BFC874A41614C840181B1E89CD7AC2A2F9B"),
    "destructor": (
        0x001F2430, 0x6C,
        "36CF4E99B833AA7773B7BF2EA869EA58092BF5047748463FC9AA2D6845302A7D"),
    "deep clone": (
        0x001F24A0, 0x5B8,
        "A694280BE699A1C1DCA6FD8108DB6B2C245222A629FE74486DD5822302E00D13"),
    "factory": (
        0x001F2FC0, 0x52C,
        "0AFBF63539F80A9CC7F4A1D2763D2E6243FDAB88CD4E7CD9AEA3BD0B3D7D56C0"),
    "pass-layer update": (
        0x001700B0, 0x84,
        "7FC0468A9A2B2AF7D8CA94B604B14C8F4DEA9662F5F5F33ADB88FE6C7D92C6F7"),
    "texture-layer dispatch": (
        0x00170790, 0x08,
        "7F016BE7D752291378FDFE71F5F9C5090039A2A7480EDBE66E219223FAD7BE7D"),
    "material-texture update": (
        0x001732E0, 0xC4,
        "F17B078F94B068977E496CA45E14000DAC066032FCE70347AC40117BCFCD4EEE"),
    "renderer texture-transform slot": (
        0x001FAB00, 0x54,
        "4B38E8F453DB4AC82195A0EC20EC9016DD424631A1CEA210D86615D6E34A492C"),
}

PS2_PRIMARY_VTABLE = (
    0, 0, 0x001F2430, 0x00100810, 0x001F24A0, 0x001F20B0,
    0x001F20A0, 0x00100010, 0x00100050,
)

PS2_INTERFACE_VTABLE = (
    0, 0,
    0x001F3590, 0x001F3580, 0x001F3570, 0x001F3560, 0x001F3550,
    0x001F3540, 0x001F3530, 0x001F3520, 0x001F3510, 0x001F3500,
    0x001F34F0, 0x001F2360, 0x001F2330, 0x001F2300, 0x001F22D0,
    0x001F22A0, 0x001F2270, 0x001F2240, 0x001F2210, 0x001F21E0,
    0x001F21D0, 0x001F21C0,
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def decode_initializer(raw: bytes) -> tuple[int, ...]:
    words = struct.unpack("<14I", raw)
    address = lambda high, low: ((high & 0xFFFF) << 16) + signed16(low & 0xFFFF)
    raw_id = lambda high, low: ((high & 0xFFFF) << 16) | (low & 0xFFFF)
    return (
        raw_id(words[1], words[7]),
        raw_id(words[2], words[8]),
        address(words[3], words[9]),
        address(words[4], words[10]),
        address(words[5], words[11]),
        address(words[0], words[6]),
    )


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pc", type=Path,
        default=root / "local-data" / "pc-pristine" / "WinxClub.exe")
    parser.add_argument(
        "--ps2", type=Path,
        default=root / "local-data" / "Winx Club the game PS2" / "SLES_532.19")
    args = parser.parse_args()

    pc = args.pc.read_bytes()
    ps2 = args.ps2.read_bytes()
    image_base, pc_sections = read_pe(pc)
    sections = read_elf(ps2)
    state = [0, 0]

    def check(label: str, actual: object, expected: object) -> None:
        state[0] += 1
        ok = actual == expected
        print(f"{'OK' if ok else 'FAIL':4} {label}: {actual!r}")
        if not ok:
            state[1] += 1
            print(f"     expected: {expected!r}")

    check("PC SHA-256", sha256(pc), PC_SHA256)
    check("PS2 SHA-256", sha256(ps2), PS2_SHA256)
    check("PC deliberately has no spPS2Material class",
          b"spPS2Material\0" in pc, False)
    check("PS2 class string", b"spPS2Material\0" in ps2, True)
    pc_transform = image_slice(
        pc, pc_sections, 0x004BB590 - image_base, 0xB4)
    check("PC renderer texture-transform body",
          digest(pc_transform),
          "47456076C0C14C5CF01F9589E83C9847BA873231E44A7D0110A235B3E0DFE89F")
    check("PC texture transform caches one 64-byte matrix per stage",
          bytes.fromhex("C1 E0 06") in pc_transform
          and bytes.fromhex("DC F0 00 00") in pc_transform, True)
    check("PC texture transform uses D3DTS_TEXTURE0 and device slot +0xb0",
          bytes.fromhex("83 C3 10") in pc_transform
          and bytes.fromhex("FF 92 B0 00 00 00") in pc_transform, True)

    for name, (address, size, expected) in PS2_BODIES.items():
        check(name, digest(image_slice(ps2, sections, address, size)), expected)

    check(
        "registration class/base/string/base-record/factory/record",
        decode_initializer(image_slice(ps2, sections, 0x00485210, 0x38)),
        (0x0F507BC8, 0x5C0314C5, 0x0045BA78,
         0x004A9610, 0x001F2FC0, 0x004B79C0))
    check(
        "primary vtable header",
        struct.unpack("<9I", image_slice(ps2, sections, 0x004916E0, 36)),
        PS2_PRIMARY_VTABLE)
    check(
        "material interface vtable",
        struct.unpack("<24I", image_slice(ps2, sections, 0x00491704, 96)),
        PS2_INTERFACE_VTABLE)

    factory = image_slice(ps2, sections, 0x001F2FC0, 0x52C)
    check("factory allocates exact 0xd0 bytes",
          bytes.fromhex("D0 00 04 24") in factory, True)
    check("factory installs both PS2 material vtables",
          bytes.fromhex("E0 16 42 24") in factory
          and bytes.fromhex("04 17 63 24") in factory, True)
    check("factory writes all four RGBA tails",
          all(struct.pack("<H", offset) in factory
              for offset in (0x80, 0x90, 0xA0, 0xB0)), True)

    copy = image_slice(ps2, sections, 0x001F20B0, 0x104)
    check("copy transfers tail from +0x80 through +0xc0",
          bytes.fromhex("80 00 20 C6 80 00 00 E6") in copy
          and bytes.fromhex("C0 00 20 C6 C0 00 00 E6") in copy, True)
    check("power accessors use +0xc0",
          image_slice(ps2, sections, 0x001F21C0, 8)
              == bytes.fromhex("08 00 E0 03 C0 00 80 C4")
          and image_slice(ps2, sections, 0x001F21D0, 8)
              == bytes.fromhex("08 00 E0 03 C0 00 8C E4"), True)

    pass_update = image_slice(ps2, sections, 0x001700B0, 0x84)
    check("pass update reads layer count +0x14 and first layer +0x18",
          bytes.fromhex("14 00 83 8C") in pass_update
          and bytes.fromhex("18 00 04 8E") in pass_update, True)
    check("pass update supports -1 as per-layer stage selection",
          bytes.fromhex("FF FF 02 24") in pass_update, True)
    texture_update = image_slice(ps2, sections, 0x001732E0, 0xC4)
    check("texture update sends UV matrix +0x48 through renderer slot 23",
          bytes.fromhex("48 00 26 26") in texture_update
          and bytes.fromhex("64 00 39 8F") in texture_update, True)

    print(f"SUMMARY: {state[0] - state[1]}/{state[0]} checks passed")
    return 1 if state[1] else 0


if __name__ == "__main__":
    raise SystemExit(main())
