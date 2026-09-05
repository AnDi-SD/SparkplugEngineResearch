#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for model-to-mesh submission."""

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


PC_MODEL_VTABLE = (
    0x00479F90, 0x005B7A00, 0x00479F40, 0x00479E60,
    0x00479A80, 0x00408350, 0x00408370, 0x00479B00,
    0x00423FD0, 0x00479DC0, 0x004240D0, 0x00479D20,
    0x00479D40, 0x0048EAA0, 0x00479DA0,
)

PS2_MODEL_VTABLE_HEADER = (
    0, 0,
    0x0015A9F0, 0x00100810, 0x0015AAB0, 0x0015A4D0,
    0x00159F70, 0x00100010, 0x00100050, 0x0015A7A0,
    0x001A9840, 0x0015A640, 0x001A9790, 0x0015A770,
    0x0015A700, 0x00159F80, 0x0015A6D0,
)

BODIES = {
    "PC spModel render": (
        "pc", 0x00479DC0, 0x51,
        "90F9E1718C62A3BEFBC81FB9DEF8DAAA98578D59E2665723DA45DE38B886040E"),
    "PC renderer submit mesh": (
        "pc", 0x004BC670, 0x68,
        "0CEC271EA50B977EEB16EEAE4E8016CCD2802928388F75F7AFAEB56EE183B7F1"),
    "PC protected submit bridge": (
        "pc", 0x004BC4A0, 0x0A,
        "040143DEED624F3DA49B62BBF417C750DD5B293176F2AA8E950CEC84EC7A0372"),
    "PC open D3D draw wrapper": (
        "pc", 0x004BC290, 0x178,
        "16C4C71282241ED539AF4B2950416E4D3BA5942E7F18493C3F668CB9C2753972"),
    "PS2 spModel render": (
        "ps2", 0x0015A640, 0x8C,
        "C4483B495A9C2141F41894E687EEAA807ED007BF358AE39A0E0E5D65571D8BD9"),
    "PS2 renderer submit mesh": (
        "ps2", 0x001FF6A0, 0x248,
        "9E099504D4CE5BFDE1CEACE9A5AE8DA565AF488F99B45FEA24D3861F49AC09A5"),
    "PS2 render-node dispatch": (
        "ps2", 0x001AA810, 0x20C,
        "21CBC5792261B02AF3AE719287208C718E3F50BB7AAAA7BDB33F49EF3B3F0072"),
    "PS2 renderable pre-render": (
        "ps2", 0x001A9840, 0x198,
        "59924020E519002FDD3245E85C9B98BD7EF551A5D726584F48B94E8534B09D7B"),
    "PS2 renderable post-render": (
        "ps2", 0x001A9790, 0xB0,
        "10AA2DCB0D7D617658DB008CE5B609CF2F61AEFA59CDBB8578015D2C4D074119"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def rel32_target(address: int, instruction: bytes, displacement_at: int) -> int:
    displacement = struct.unpack_from("<i", instruction, displacement_at)[0]
    return address + displacement_at + 4 + displacement


def mips_jump_target(word: int, address: int) -> int:
    return ((address + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)


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
    checks = 0
    failures: list[str] = []

    def check(label: str, actual: object, expected: object) -> None:
        nonlocal checks
        checks += 1
        ok = actual == expected
        print(f"{'OK' if ok else 'FAIL':4} {label}: {actual!r}")
        if not ok:
            failures.append(f"{label}: expected {expected!r}, got {actual!r}")

    def pc_slice(address: int, size: int) -> bytes:
        return image_slice(pc, pc_sections, address - image_base, size)

    def ps2_slice(address: int, size: int) -> bytes:
        return image_slice(ps2, ps2_sections, address, size)

    check("PC SHA-256", sha256(pc), PC_SHA256)
    check("PS2 SHA-256", sha256(ps2), PS2_SHA256)

    for label, (platform, address, size, expected) in BODIES.items():
        data = pc_slice(address, size) if platform == "pc" else ps2_slice(address, size)
        check(label, digest(data), expected)

    check(
        "PC spModel vtable",
        struct.unpack("<15I", pc_slice(0x006EAA58, 15 * 4)),
        PC_MODEL_VTABLE)
    check(
        "PS2 spModel vtable header",
        struct.unpack("<17I", ps2_slice(0x0048E250, 17 * 4)),
        PS2_MODEL_VTABLE_HEADER)
    check("PC model render overrides pure slot", PC_MODEL_VTABLE[9], 0x00479DC0)
    check("PS2 model render overrides pure slot",
          PS2_MODEL_VTABLE_HEADER[11], 0x0015A640)
    check("PC classifier-equivalent slot is a no-op",
          PC_MODEL_VTABLE[13], 0x0048EAA0)
    check("PS2 classifier slot is concrete",
          PS2_MODEL_VTABLE_HEADER[15], 0x00159F80)

    pc_interface = struct.unpack(
        "<29I", pc_slice(0x006F28A0, 29 * 4))
    ps2_bodies = (
        0x001FBE40, 0x001FBD90, 0x001FBD80, 0x00201BC0,
        0x00201900, 0x00200BD0, 0x00200470, 0x001FE910,
        0x001FBD30, 0x001FF6A0, 0x001FB8C0, 0x001FF4B0,
        0x001FBA40, 0x001FB260, 0x001F6F50, 0x001FC1A0,
        0x001FA9D0, 0x001FA9C0, 0x001FA9B0, 0x001FA880,
        0x001FA870, 0x001FA860, 0x001FAB60, 0x001FAB00,
        0x001FA9E0, 0x001FB1E0, 0x001FB1F0, 0x001FA850,
        0x001FEDB0,
    )
    check("PC renderer slot 9 target", pc_interface[9], 0x004BC670)
    check("PS2 renderer slot 9 body", ps2_bodies[9], 0x001FF6A0)
    ps2_slot9_thunk = ps2_slice(0x001FCD00, 8)
    thunk_word, delay_word = struct.unpack("<II", ps2_slot9_thunk)
    check("PS2 slot 9 thunk target",
          mips_jump_target(thunk_word, 0x001FCD00), 0x001FF6A0)
    check("PS2 slot 9 thunk adjusts this by -0x18", delay_word, 0x2484FFE8)

    pc_model = pc_slice(0x00479DC0, 0x51)
    check("PC model calls pre-render slot +0x20",
          bytes.fromhex("FF 50 20") in pc_model, True)
    check("PC model passes base mesh at +0x58",
          bytes.fromhex("8B 46 58") in pc_model, True)
    check("PC model calls renderer-interface slot +0x24",
          bytes.fromhex("FF 52 24") in pc_model, True)
    check("PC model calls post-render slot +0x28",
          bytes.fromhex("FF 50 28") in pc_model, True)

    ps2_model = ps2_slice(0x0015A640, 0x8C)
    check("PS2 model calls pre-render slot +0x28",
          bytes.fromhex("28 00 39 8F") in ps2_model, True)
    check("PS2 model loads base mesh at +0x50",
          bytes.fromhex("50 00 45 8E") in ps2_model, True)
    check("PS2 model calls renderer-interface slot +0x2c",
          bytes.fromhex("2C 00 39 8F") in ps2_model, True)
    check("PS2 model calls post-render slot +0x30",
          bytes.fromhex("30 00 39 8F") in ps2_model, True)

    pc_submit = pc_slice(0x004BC670, 0x68)
    mesh_reads = (
        bytes.fromhex("8B 50 44"), bytes.fromhex("8B 68 48"),
        bytes.fromhex("8B 58 4C"), bytes.fromhex("8B 68 50"),
        bytes.fromhex("8B 40 54"), bytes.fromhex("8B 68 58"),
        bytes.fromhex("8B 70 74"), bytes.fromhex("8B 68 78"),
        bytes.fromhex("8B 68 7C"), bytes.fromhex("8B B8 84 00 00 00"),
    )
    check("PC submit reads the confirmed spDXMesh draw fields",
          all(pattern in pc_submit for pattern in mesh_reads), True)
    call_at = pc_submit.index(bytes.fromhex("E8 D2 FD FF FF"))
    check("PC submit reaches protected bridge",
          rel32_target(0x004BC670 + call_at,
                       pc_submit[call_at:call_at + 5], 1),
          0x004BC4A0)
    check("PC protected bridge remains a SecuROM indirect jump",
          pc_slice(0x004BC4A0, 0x0A),
          bytes.fromhex("53 56 8B F1 FF 25 A4 14 3B 01"))

    pc_draw = pc_slice(0x004BC290, 0x178)
    check("PC wrapper reaches D3D declaration/FVF endpoints",
          all(pattern in pc_draw for pattern in (
              bytes.fromhex("FF 92 70 01 00 00"),
              bytes.fromhex("FF 92 78 01 00 00"))), True)
    check("PC wrapper reaches D3D stream/index endpoints",
          all(pattern in pc_draw for pattern in (
              bytes.fromhex("FF 92 AC 01 00 00"),
              bytes.fromhex("FF 92 B4 01 00 00"))), True)
    check("PC wrapper reaches DrawPrimitive and DrawIndexedPrimitive",
          all(pattern in pc_draw for pattern in (
              bytes.fromhex("FF 92 44 01 00 00"),
              bytes.fromhex("FF 92 48 01 00 00"))), True)

    ps2_submit = ps2_slice(0x001FF6A0, 0x248)
    check("PS2 submit follows mesh +0x50 to payload +0x40",
          bytes.fromhex("50 00 A2 8C 40 00 42 8C") in ps2_submit, True)
    check("PS2 submit binds mesh buffer references +0x4c/+0x48",
          bytes.fromhex("4C 00 12 8E") in ps2_submit
          and bytes.fromhex("48 00 10 8E") in ps2_submit, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
