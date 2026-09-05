#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for spMaterialData, spFog and fog binding."""

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


PC_BODIES = {
    "spMaterial registration": (
        0x006D2930, 0x23,
        "BA42D0F0D678F59A90CB750C5253E759E97B1B87DFD75022F30B5734F54BBF02"),
    "spMaterialData registration": (
        0x006D1D90, 0x27,
        "12BCE0863606F8B06BDB15740EC572D9B7C1EF7BCAA97B5BE4568B25B00B8365"),
    "spFog registration": (
        0x006D1A90, 0x27,
        "20474145C3489C3BD743B8756FAEC0D5003944C10FB2FF945246950C1308D30A"),
    "renderer SetFog slot body": (
        0x004AD390, 0x125,
        "D6C43F4FE0BDD3E08D0639A775E6E7585770174EBFCCDB5A86985FFE577F385B"),
}


PS2_BODIES = {
    "spMaterial registration": (
        0x00482B10, 0x34,
        "1B3D52B5FBCC179A774A109797AD1413FD91082D5E37420F9B7E3D210AA7BADA"),
    "spMaterialData registration": (
        0x00480ED4, 0x34,
        "37522158A9ED2A773AB39BDAB58850D277F6B4C805288CD430F971570B157F42"),
    "spFog registration": (
        0x00480B5C, 0x34,
        "124FE6EAE5AF4CD7DC633CC81571D40ED1E553BCB74396DB2EBAE70EF0449702"),
    "spMaterial constructor": (
        0x0016F570, 0xDC,
        "0DCA61FCB208A113BEC8659B82A57412AAC5836F31CD82A4BC3E35230E7BAE4D"),
    "spMaterial render-state defaults helper": (
        0x0017B270, 0x28,
        "B21A9B943AD70C73178BCF437D57A5892CCC100B52C97116F8B545E2B1988E0C"),
    "spMaterialData factory": (
        0x00134AB0, 0x38,
        "65B4D16D4E3778CEEEDD481596C4C0FEEA9017C3FD0F1E4672C5405F0EC2CE31"),
    "spMaterialData constructor": (
        0x0016F9E0, 0x500,
        "F80BD5EF97C0AFEA0AC11413C9F0EF1E13F4E017A6FE42D09967AFAEB9127DCE"),
    "spMaterialData blank clone": (
        0x001349F0, 0xC0,
        "5B9DD9C993283393E2D1B44C04DE9904849288FC2AD40012A2C304BED2B2859C"),
    "spMaterialData copy-success stub": (
        0x0016F950, 0x08,
        "5CE5AD86D452C4D2422BD63E15223D5D6B3DFB77F224A88C1F476E9FB34E359D"),
    "spFog factory/default constructor": (
        0x00135820, 0x88,
        "01AE2ADAC062B596180D9D1B05FA3142165A1171DDA34C0C5E840C5666BED1EF"),
    "spFog blank clone": (
        0x00135720, 0xFC,
        "B470E338F1083526DF850DCC9BD5CB18609570F35F97E8B06301B6F0BB2F4725"),
    "renderer SetFog slot body": (
        0x001FB1F0, 0x68,
        "3E493AF41317D9EC7A4B4BB24EF3544E874505CBF790D130E6F323BFC4B1D08D"),
    "black/white color globals initializer prefix": (
        0x0047F2A0, 0x50,
        "687E267FFAB833F5C3AE73783FB943DA53F8C146E4DCAB8645CFF2EA79ACC581"),
}


PC_MATERIAL_VTABLE = (
    0x00423B30, 0x005B7A00, 0x004A1BF0, 0x00423880,
    0x00423830, 0x00408350, 0x00408370,
)
PC_MATERIAL_DATA_VTABLE = (
    0x004357C0, 0x005B7A00, 0x0041ACF0, 0x005A7DB0,
    0x004356E0, 0x00408350, 0x00408370,
)
PC_FOG_VTABLE = (
    0x0041A8B0, 0x005B7A00, 0x0041A8E0, 0x00413120,
    0x00419E80, 0x00408350, 0x00408370,
)

PS2_MATERIAL_VTABLE = (
    0, 0, 0x0016F480, 0x00100810, 0x0016F650, 0x0016F1B0,
    0x0016ED00, 0x00100010, 0x00100050,
)
PS2_MATERIAL_DATA_VTABLE = (
    0, 0, 0x0016F970, 0x00100810, 0x001349F0, 0x0016F950,
    0x00135AF0, 0x00100010, 0x00100050,
)
PS2_FOG_VTABLE = (
    0, 0, 0x00135C00, 0x00100810, 0x00135720, 0x00105DC0,
    0x00135BF0, 0x00100010, 0x00100050,
)
PS2_MATERIAL_DATA_ACCESSORS = (
    0, 0, 0x00135DF0, 0x00135DE0, 0x00135DD0, 0x00135DC0,
    0x00135DB0, 0x00135DA0, 0x00135D90, 0x00135D80,
    0x00135D70, 0x00135D60, 0x00135D50,
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


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

    check("PC SHA-256", sha256(pc), PC_SHA256)
    check("PS2 SHA-256", sha256(ps2), PS2_SHA256)
    for name in (b"spMaterial\0", b"spMaterialData\0", b"spFog\0"):
        text = name[:-1].decode()
        check(f"PC class string {text}", name in pc, True)
        check(f"PS2 class string {text}", name in ps2, True)

    for label, (address, size, expected) in PC_BODIES.items():
        check(
            f"PC {label}",
            digest(image_slice(
                pc, pc_sections, address - image_base, size)),
            expected)
    for label, (address, size, expected) in PS2_BODIES.items():
        check(
            f"PS2 {label}",
            digest(image_slice(ps2, ps2_sections, address, size)),
            expected)

    check(
        "PC spMaterial primary vtable",
        struct.unpack("<7I", image_slice(
            pc, pc_sections, 0x006DC984 - image_base, 7 * 4)),
        PC_MATERIAL_VTABLE)
    check(
        "PC spMaterialData primary vtable",
        struct.unpack("<7I", image_slice(
            pc, pc_sections, 0x006DE9FC - image_base, 7 * 4)),
        PC_MATERIAL_DATA_VTABLE)
    check(
        "PC spFog primary vtable",
        struct.unpack("<7I", image_slice(
            pc, pc_sections, 0x006DBD04 - image_base, 7 * 4)),
        PC_FOG_VTABLE)
    check(
        "PS2 spMaterial vtable header",
        struct.unpack("<9I", image_slice(
            ps2, ps2_sections, 0x0048E870, 9 * 4)),
        PS2_MATERIAL_VTABLE)
    check(
        "PS2 spMaterialData vtable header",
        struct.unpack("<9I", image_slice(
            ps2, ps2_sections, 0x0048D8A0, 9 * 4)),
        PS2_MATERIAL_DATA_VTABLE)
    check(
        "PS2 spFog vtable header",
        struct.unpack("<9I", image_slice(
            ps2, ps2_sections, 0x0048DCF0, 9 * 4)),
        PS2_FOG_VTABLE)
    check(
        "PS2 material-data secondary color accessors",
        struct.unpack("<13I", image_slice(
            ps2, ps2_sections, 0x0048D8C4, 13 * 4)),
        PS2_MATERIAL_DATA_ACCESSORS)

    material_factory = image_slice(ps2, ps2_sections, 0x00134AB0, 0x38)
    check("PS2 material-data allocation is 0xd0",
          bytes.fromhex("D0 00 04 24") in material_factory, True)
    fog_factory = image_slice(ps2, ps2_sections, 0x00135820, 0x88)
    check("PS2 fog allocation is 0x28",
          bytes.fromhex("28 00 04 24") in fog_factory, True)
    check("PS2 fog defaults include type/start zero and end/density one",
          bytes.fromhex("14 00 00 AE") in fog_factory
          and bytes.fromhex("1C 00 00 AE") in fog_factory
          and bytes.fromhex("20 00 02 AE") in fog_factory
          and bytes.fromhex("24 00 02 AE") in fog_factory, True)

    color_init = image_slice(ps2, ps2_sections, 0x0047F2A0, 0x50)
    check("PS2 shared black color is opaque ARGB black",
          bytes.fromhex("00 FF 14 3C") in color_init
          and bytes.fromhex("B0 6C 74 AC") in color_init, True)
    check("PS2 shared white color is opaque ARGB white",
          bytes.fromhex("FF FF 13 24") in color_init
          and bytes.fromhex("B8 6C 73 AC") in color_init, True)

    pc_md_bridge = image_slice(
        pc, pc_sections, 0x0041A390 - image_base, 6)
    pc_fog_bridge = image_slice(
        pc, pc_sections, 0x00419E90 - image_base, 6)
    check("PC material-data factory remains an exact protected thunk",
          pc_md_bridge, bytes.fromhex("FF 25 94 25 3B 01"))
    check("PC fog factory remains an exact protected thunk",
          pc_fog_bridge, bytes.fromhex("FF 25 24 26 3B 01"))

    pc_set_fog = image_slice(
        pc, pc_sections, 0x004AD390 - image_base, 0x125)
    check("PC fog binder reads type/color/start/end/density",
          all(fragment in pc_set_fog for fragment in (
              bytes.fromhex("8B 47 14"), bytes.fromhex("8B 47 18"),
              bytes.fromhex("8B 4F 1C"), bytes.fromhex("8B 47 20"),
              bytes.fromhex("8B 47 24"))), True)
    check("PC fog binder reaches D3D fog states 0x1c and 0x22..0x26",
          all(bytes((0x6A, value)) in pc_set_fog
              for value in (0x1C, 0x22, 0x23, 0x24, 0x25, 0x26)), True)
    ps2_set_fog = image_slice(ps2, ps2_sections, 0x001FB1F0, 0x68)
    check("PS2 fog binder stores fog and distinguishes linear type 3",
          bytes.fromhex("60 97 65 AC") in ps2_set_fog
          and bytes.fromhex("14 00 42 8C") in ps2_set_fog
          and bytes.fromhex("03 00 03 24") in ps2_set_fog, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
