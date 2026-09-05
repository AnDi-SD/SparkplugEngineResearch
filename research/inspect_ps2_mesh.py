#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for the native spPS2Mesh leaf."""

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
        0x00485090, 0x38,
        "48B11794434585A2EE931203B8309A15E499E70C8A0825E476A9E45D29D13E9D"),
    "registration getter": (
        0x001EF310, 0x0C,
        "B077B1E3889CFA7FC79D6177FE64642B4B42C886554CD6293DAAC31035865199"),
    "buffer conversion": (
        0x001EF320, 0x90,
        "E8AA1EB34E0700DEC94C83C185B7771800E7E685EC8693014963FCCD94F446C6"),
    "ready result": (
        0x001EF3B0, 0x08,
        "5CE5AD86D452C4D2422BD63E15223D5D6B3DFB77F224A88C1F476E9FB34E359D"),
    "prepared-data attach": (
        0x001EF3C0, 0x74,
        "FD53AB95C54C418FA9B99A0CAEFD175FF5DF5D89A5EDD8227DB47660BDED880F"),
    "prepared-data release": (
        0x001EF440, 0x48,
        "D1CF9758A246F2EE0352B30C697CCED91429D9BA984FCFE495F08389149092C2"),
    "destructor": (
        0x001EF490, 0x98,
        "B2A2983355795BEDEBCFEA1F206540D7AA8335A3BFB150447DA851BCA3C3FC33"),
    "blank clone": (
        0x001EF530, 0x10C,
        "F5F89D8F40677E886D020F63B55611D2AAF8BC829BE0C272079B081E0303C477"),
    "factory": (
        0x001EF6E0, 0x98,
        "3FA8F424F32F771CF56505623C1706E60D7BC0C8B841E1D046E0647305A75CE7"),
    "renderer packet preparation": (
        0x001FF8F0, 0x194,
        "5D34F835923EA4BF9C18AEA0E5A2F4FF84C24B2AB4A36269227FA43819F0151C"),
}

PS2_PRIMARY_VTABLE = (
    0, 0, 0x001EF490, 0x00100810, 0x001EF530, 0x00105DC0,
    0x001EF310, 0x00100010, 0x00100050,
)

PS2_SECONDARY_VTABLE = (
    0, 0, 0x001EF790, 0x001EF780, 0x00159C80,
    0x001EF3C0, 0x001EF3B0, 0x001EF320, 0x001EF440,
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
    ps2_sections = read_elf(ps2)
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
    check("PC deliberately has no spPS2Mesh class", b"spPS2Mesh\0" in pc, False)
    check("PS2 class string", b"spPS2Mesh\0" in ps2, True)

    for name, (address, size, expected) in PS2_BODIES.items():
        check(name, digest(image_slice(ps2, ps2_sections, address, size)), expected)

    registration = image_slice(ps2, ps2_sections, 0x00485090, 0x38)
    check(
        "registration class/base/string/base-record/factory/record",
        decode_initializer(registration),
        (0x35ED77A5, 0x67974A9C, 0x0045B958,
         0x004A8E90, 0x001EF6E0, 0x004B7380))
    check(
        "primary vtable header",
        struct.unpack("<9I", image_slice(ps2, ps2_sections, 0x00491410, 36)),
        PS2_PRIMARY_VTABLE)
    check(
        "secondary vtable header",
        struct.unpack("<9I", image_slice(ps2, ps2_sections, 0x00491434, 36)),
        PS2_SECONDARY_VTABLE)

    factory = image_slice(ps2, ps2_sections, 0x001EF6E0, 0x98)
    check("factory allocates exact 0x58 bytes",
          bytes.fromhex("58 00 04 24") in factory, True)
    check("factory clears prepared data and packet emitter before binding",
          bytes.fromhex("50 00 00 AE 54 00 00 AE") in factory, True)

    attach = image_slice(ps2, ps2_sections, 0x001EF3C0, 0x74)
    check("attach stores owned data at +0x50",
          bytes.fromhex("50 00 30 AE") in attach, True)
    check("attach caches data +0x34 as primitive count +0x48",
          bytes.fromhex("34 00 63 8C 48 00 23 AE") in attach, True)
    check("attach caches data +0x30 as vertex count +0x4c",
          bytes.fromhex("30 00 63 8C 4C 00 23 AE") in attach, True)

    prepare = image_slice(ps2, ps2_sections, 0x001FF8F0, 0x194)
    check("draw preparation follows native mesh data at +0x50",
          bytes.fromhex("50 00 AC 8C") in prepare
          and bytes.fromhex("38 00 8C 8D") in prepare, True)
    check("draw preparation invokes packet-emitter slots 4, 3 and 5",
          all(fragment in prepare for fragment in (
              bytes.fromhex("10 00 39 8F"),
              bytes.fromhex("0C 00 39 8F"),
              bytes.fromhex("14 00 39 8F"))), True)

    print(f"SUMMARY: {state[0] - state[1]}/{state[0]} checks passed")
    return 1 if state[1] else 0


if __name__ == "__main__":
    raise SystemExit(main())
