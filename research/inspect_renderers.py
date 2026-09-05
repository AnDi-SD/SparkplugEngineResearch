#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for the native renderer family."""

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


PC_INTERFACE_TARGETS = (
    0x004BC160, 0x004BC0D0, 0x004A1BF0, 0x004BB950,
    0x004BB9D0, 0x004BB980, 0x004BB9F0, 0x004BD380,
    0x004BD810, 0x004BC670, 0x004AD7A0, 0x004BC9A0,
    0x004BBAE0, 0x004BBB20, 0x004BBB60, 0x004AD330,
    0x004AD350, 0x004AD360, 0x004AD370, 0x004AD640,
    0x004AD660, 0x004AD680, 0x004BBA80, 0x004BB590,
    0x004BB4B0, 0x004AD380, 0x004AD390, 0x004AD4D0,
    0x004BD700,
)

PS2_INTERFACE_THUNKS = (
    0x001FCD80, 0x001FCD90, 0x001FCD70, 0x001FCD60,
    0x001FCD50, 0x001FCD40, 0x001FCD30, 0x001FCD20,
    0x001FCD10, 0x001FCD00, 0x001FCCF0, 0x001FCCE0,
    0x001FCCD0, 0x001FCCC0, 0x001FCCB0, 0x001FCCA0,
    0x001FCC90, 0x001FCC80, 0x001FCC70, 0x001FCC60,
    0x001FCC50, 0x001FCC40, 0x001FCC30, 0x001FCC10,
    0x001FCC20, 0x001FCC00, 0x001FCBF0, 0x001FCBE0,
    0x001FCBD0,
)

PS2_INTERFACE_BODIES = (
    0x001FBE40, 0x001FBD90, 0x001FBD80, 0x00201BC0,
    0x00201900, 0x00200BD0, 0x00200470, 0x001FE910,
    0x001FBD30, 0x001FF6A0, 0x001FB8C0, 0x001FF4B0,
    0x001FBA40, 0x001FB260, 0x001F6F50, 0x001FC1A0,
    0x001FA9D0, 0x001FA9C0, 0x001FA9B0, 0x001FA880,
    0x001FA870, 0x001FA860, 0x001FAB60, 0x001FAB00,
    0x001FA9E0, 0x001FB1E0, 0x001FB1F0, 0x001FA850,
    0x001FEDB0,
)

PC_BODIES = {
    "common registration initializer": (
        0x006D35E0, 0x23,
        "DA1795A7F741ADEB487901E9D45B052108A5D87E3E3364342A72FA01AE241EFB"),
    "DX registration initializer": (
        0x006D50D0, 0x23,
        "BD659A977258BC4BB7D022CAB57BB46E35C64A076382F0E1CE2B627699289AA6"),
    "PC registration initializer": (
        0x006D58B0, 0x26,
        "059A09A77A81D13F7A96DB056477518A50D27F8EA3F80FA322735ADE9C06E644"),
    "state-cache invalidator": (
        0x00454940, 0x27,
        "EC5BCE5E007B860331FCB35CB0D42888A44B1FD92C2AF3423F86941896C3FB09"),
    "PC leaf factory": (
        0x004C5AB0, 0x5A,
        "4DDF18EB1637E9EC05DCA13EC3C6CFF60DBEC96ED79E5E4C9FAA9AD6F00E2FDD"),
}

PS2_BODIES = {
    "common registration initializer": (
        0x00482F50, 0x34,
        "56A01529CA49C3F319AC993B17FB65B7588744170F100F6FB4F11315CD111552"),
    "PS2 registration prefix": (
        0x00485350, 0x64,
        "C7E9DA3FCF26CA20A7FB7E42494A57B27D569B79F2EE53CBACBF7E00E6F5EEBA"),
    "state-cache invalidator": (
        0x00179E60, 0x4C,
        "936B7FED8A87210001AE0F08992657C17320B09D46AA39DAE1A64C6B6964B2F9"),
    "PS2 leaf factory": (
        0x001FCB80, 0x40,
        "2DBA890DDD561748B73D036CE2B881676BEEE4726FBB5B70F58628EBEDBF9082"),
}

# Camera/material call sites and D3D9/GS endpoints give these slots concrete
# semantics. Hash the complete bodies so later annotation work cannot silently
# move a label onto a neighbouring vtable entry.
PC_GRAPHICS_BODIES = {
    "bind cube render target (slot 0)": (
        0x004BC160, 0x8D,
        "778D071BACD42130E6F017CD1EB3D22A4986334A4FF0B5C5EE1C40F616AF6AE8"),
    "bind ordinary render target (slot 1)": (
        0x004BC0D0, 0x88,
        "7AC8220AA2D09F9DBD67D3E825B8F00FE4A8B7BCAFD32648086D2402968EDBAF"),
    "begin scene (slot 3)": (
        0x004BB950, 0x23,
        "E5BC592C3EDF3AF53DE7DF0485728DD32B4F9CC4E4D223FDA8830F9E0DD31255"),
    "end scene (slot 4)": (
        0x004BB9D0, 0x20,
        "D129F9F3E7F38628DCC160B442FB3CFA5C0D477B7E3056FD631520599F897A5F"),
    "clear (slot 5)": (
        0x004BB980, 0x47,
        "4744366E3B153F69DAFD7525E02E5146A1EBFFE26D84756DF175502222ABC2F7"),
    "configure 2D (slot 10)": (
        0x004AD7A0, 0xF6,
        "37A96D767111E6EA3FBAE21763D97806865468CB3897ABD5E20BA2B667E6AD81"),
    "projection matrix (slot 12)": (
        0x004BBAE0, 0x36,
        "8F5065F908D7F6F2D005F93439C42CB42B2224E8986329C241E17DAEA511B3DB"),
    "view matrix (slot 13)": (
        0x004BBB20, 0x36,
        "A07E5A5BFC47AEB0BD579A19357DD24C18F6B32EC367DA581A341F0D4EA5A8E8"),
    "world matrix (slot 14)": (
        0x004BBB60, 0x39,
        "B6909800E9F0964ED01978D517F50AB79BAF6AAEE7E17F7313F4DB1B9ED2CFD0"),
    "viewport (slot 22)": (
        0x004BBA80, 0x55,
        "0E0EB2B6F54410A69A5A8F3CAFAF7B89AF7DC51514D2F895C40EE19F364512E0"),
}

PS2_GRAPHICS_BODIES = {
    "bind ordinary render target (slot 0)": (
        0x001FBE40, 0x354,
        "2B44231B110BC2D3C52EDFF0F2C657B03683A5D515541B23804898726166C7E5"),
    "unsupported cube render target (slot 1)": (
        0x001FBD90, 0xA8,
        "EE70BFC1FB7C8590C7CFAAFC2AE94BCF0D25617AE84DAB7F47E1BDA9A8973D5E"),
    "begin scene (slot 3)": (
        0x00201BC0, 0x44,
        "F0C73CA54D0F1B8BEB05CB8FEABAD500C0C6463D6BCBA59976A44C069AB49CCD"),
    "end scene (slot 4)": (
        0x00201900, 0x2C0,
        "09F1BA3BDEDB237176F60A29A785FEED57099DED7D5D95860E855C501218CE59"),
    "clear (slot 5)": (
        0x00200BD0, 0xD28,
        "2335EDC382723CB601B19E78F8034018DFEABA6ECBA66A1FFD2A613A898A6B3A"),
    "configure 2D (slot 10)": (
        0x001FB8C0, 0x17C,
        "B20FAC187783D6A24920794690FB9A5F9EEADE10EE99C33E246052E8667273F2"),
    "projection matrix (slot 12)": (
        0x001FBA40, 0x2E8,
        "F4983A74C6FA42D74F13B53D49365EB062CC39A69CFFBD9CA485C5910CA066A0"),
    "view matrix (slot 13)": (
        0x001FB260, 0x140,
        "FC9F531F1F1F8DA799B56A56821DF7AE4B25A5A9E991685531F99C5242FACA9B"),
    "world matrix (slot 14)": (
        0x001F6F50, 0x9C,
        "D2B55D7974A78CC16FF42D84BC2EF2794EB9CA911264DBA762C41511CE1E0B48"),
    "viewport (slot 22)": (
        0x001FAB60, 0x680,
        "8B0CED2EFCF82F42B806E6200BC1C1CF01DAAD43782CF038C5EABDC4C903CAC1"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def mips_jump_target(word: int, address: int) -> int:
    """Decode a J/JAL target in the same 256 MiB region as the delay slot."""
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

    check("PC SHA-256", sha256(pc), PC_SHA256)
    check("PS2 SHA-256", sha256(ps2), PS2_SHA256)
    for name in (b"spRenderer\0", b"spDXRenderer\0", b"spPCRenderer\0"):
        check(f"PC class string {name[:-1].decode()}", name in pc, True)
    check("PS2 class string spRenderer", b"spRenderer\0" in ps2, True)
    check("PS2 class string spPS2Renderer", b"spPS2Renderer\0" in ps2, True)
    check("PS2 has no DX renderer leaf", b"spDXRenderer\0" in ps2, False)

    for label, (address, size, expected) in PC_BODIES.items():
        check(
            f"PC {label}",
            digest(image_slice(pc, pc_sections, address - image_base, size)),
            expected)
    for label, (address, size, expected) in PS2_BODIES.items():
        check(
            f"PS2 {label}",
            digest(image_slice(ps2, ps2_sections, address, size)),
            expected)
    for label, (address, size, expected) in PC_GRAPHICS_BODIES.items():
        check(
            f"PC {label}",
            digest(image_slice(pc, pc_sections, address - image_base, size)),
            expected)
    for label, (address, size, expected) in PS2_GRAPHICS_BODIES.items():
        check(
            f"PS2 {label}",
            digest(image_slice(ps2, ps2_sections, address, size)),
            expected)

    common_reg = image_slice(pc, pc_sections, 0x006D35E0 - image_base, 0x23)
    check("PC common class ID", struct.pack("<I", 0x2D9C0296) in common_reg, True)
    check("PC common spCrossPlatform base ID",
          struct.pack("<I", 0x20A72504) in common_reg, True)
    check("PC common null factory", common_reg.startswith(b"\x6A\x00\x6A\x00"), True)
    check("PC common registration object",
          struct.pack("<I", 0x0075F8F0) in common_reg, True)

    dx_reg = image_slice(pc, pc_sections, 0x006D50D0 - image_base, 0x23)
    check("PC DX class ID", struct.pack("<I", 0x46004EE1) in dx_reg, True)
    check("PC DX direct common base ID",
          struct.pack("<I", 0x2D9C0296) in dx_reg, True)
    check("PC DX null factory", dx_reg.startswith(b"\x6A\x00\x6A\x00"), True)
    check("PC DX direct common registration",
          struct.pack("<I", 0x0075F8F0) in dx_reg, True)

    leaf_reg = image_slice(pc, pc_sections, 0x006D58B0 - image_base, 0x26)
    check("PC leaf class ID", struct.pack("<I", 0x26267C84) in leaf_reg, True)
    check("PC leaf direct DX base ID",
          struct.pack("<I", 0x46004EE1) in leaf_reg, True)
    check("PC leaf factory", struct.pack("<I", 0x004C5AB0) in leaf_reg, True)
    check("PC leaf direct DX registration",
          struct.pack("<I", 0x007639C0) in leaf_reg, True)

    pc_invalidator = image_slice(
        pc, pc_sections, 0x00454940 - image_base, 0x27)
    check("PC invalidator starts twelve-state fill at +0xc868",
          bytes.fromhex("B9 0C 00 00 00 8D BA 68 C8 00 00 F3 AB")
          in pc_invalidator, True)
    check("PC invalidator starts 72-entry texture fill at +0xc898",
          bytes.fromhex("B9 48 00 00 00 8D BA 98 C8 00 00 F3 AB")
          in pc_invalidator, True)
    pc_factory = image_slice(pc, pc_sections, 0x004C5AB0 - image_base, 0x5A)
    check("PC leaf allocation size 0xf368",
          bytes.fromhex("68 68 F3 00 00") in pc_factory, True)
    check("PC exact surviving DX source path",
          b"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXRenderer_Init.cpp\0"
          in pc, True)
    check(
        "PC leaf 29-slot platform interface",
        struct.unpack(
            "<29I", image_slice(
                pc, pc_sections, 0x006F28A0 - image_base, 29 * 4)),
        PC_INTERFACE_TARGETS,
    )
    check("PC leaf makes formerly pure interface slot 2 concrete",
          PC_INTERFACE_TARGETS[2], 0x004A1BF0)
    check("PC target operations use cube then ordinary slots",
          PC_INTERFACE_TARGETS[:2], (0x004BC160, 0x004BC0D0))
    check("PC projection/view/world slots are consecutive",
          PC_INTERFACE_TARGETS[12:15],
          (0x004BBAE0, 0x004BBB20, 0x004BBB60))
    pc_projection = image_slice(
        pc, pc_sections, 0x004BBAE0 - image_base, 0x36)
    pc_view = image_slice(pc, pc_sections, 0x004BBB20 - image_base, 0x36)
    pc_world = image_slice(pc, pc_sections, 0x004BBB60 - image_base, 0x39)
    check("PC projection reaches D3D SetTransform state 3",
          bytes.fromhex("6A 03 56 FF 91 B0 00 00 00") in pc_projection,
          True)
    check("PC view reaches D3D SetTransform state 2",
          bytes.fromhex("6A 02 56 FF 91 B0 00 00 00") in pc_view,
          True)
    check("PC world reaches D3D SetTransform state 0x100",
          bytes.fromhex("68 00 01 00 00 56 FF 91 B0 00 00 00") in pc_world,
          True)
    pc_viewport = image_slice(
        pc, pc_sections, 0x004BBA80 - image_base, 0x55)
    check("PC viewport reaches D3D SetViewport",
          bytes.fromhex("FF 91 BC 00 00 00") in pc_viewport, True)

    ps2_common_reg = image_slice(ps2, ps2_sections, 0x00482F50, 0x34)
    check("PS2 common class ID halves",
          all(part in ps2_common_reg for part in (
              bytes.fromhex("9C 2D 03 3C"), bytes.fromhex("96 02 65 34"))),
          True)
    check("PS2 common direct spCrossPlatform base halves",
          all(part in ps2_common_reg for part in (
              bytes.fromhex("A7 20 02 3C"), bytes.fromhex("04 25 46 34"))),
          True)
    check("PS2 common null factory/callback",
          ps2_common_reg.endswith(
              bytes.fromhex("2D 48 00 00 6C 45 04 08 2D 50 00 00")),
          True)

    ps2_leaf_reg = image_slice(ps2, ps2_sections, 0x00485350, 0x64)
    check("PS2 leaf class ID halves",
          all(part in ps2_leaf_reg for part in (
              bytes.fromhex("36 30 03 3C"), bytes.fromhex("B8 52 65 34"))),
          True)
    check("PS2 leaf direct common base ID halves",
          all(part in ps2_leaf_reg for part in (
              bytes.fromhex("9C 2D 02 3C"), bytes.fromhex("96 02 46 34"))),
          True)
    check("PS2 leaf factory address halves",
          all(part in ps2_leaf_reg for part in (
              bytes.fromhex("20 00 09 3C"), bytes.fromhex("80 CB 29 25"))),
          True)

    ps2_factory = image_slice(ps2, ps2_sections, 0x001FCB80, 0x40)
    check("PS2 leaf allocation alignment 16",
          bytes.fromhex("10 00 05 24") in ps2_factory, True)
    check("PS2 leaf allocation size 0x19d00",
          bytes.fromhex("01 00 02 3C") in ps2_factory
          and bytes.fromhex("00 9D 44 34") in ps2_factory, True)
    check("PS2 leaf factory calls constructor",
          struct.pack("<I", 0x0C000000 | (0x001FC2E0 >> 2))
          in ps2_factory, True)

    ps2_invalidator = image_slice(ps2, ps2_sections, 0x00179E60, 0x4C)
    check("PS2 invalidator starts at +0xc9d0 and writes 48 bytes",
          bytes.fromhex("D0 C9 01 34") in ps2_invalidator
          and bytes.fromhex("30 00 06 24") in ps2_invalidator, True)
    check("PS2 invalidator continues at +0xca00 and writes 384 bytes",
          bytes.fromhex("00 CA 01 34") in ps2_invalidator
          and bytes.fromhex("80 01 06 24") in ps2_invalidator, True)

    check(
        "PS2 leaf interface header and 29 thunk slots",
        struct.unpack(
            "<31I", image_slice(ps2, ps2_sections, 0x00491950, 31 * 4)),
        (0, 0, *PS2_INTERFACE_THUNKS),
    )
    decoded_bodies = []
    delay_slots = []
    opcodes = []
    for thunk in PS2_INTERFACE_THUNKS:
        word, delay = struct.unpack(
            "<II", image_slice(ps2, ps2_sections, thunk, 8))
        opcodes.append(word >> 26)
        decoded_bodies.append(mips_jump_target(word, thunk))
        delay_slots.append(delay)
    check("PS2 interface entries are unconditional J thunks",
          tuple(opcodes), (2,) * 29)
    check("PS2 interface thunks all adjust this by -0x18",
          tuple(delay_slots), (0x2484FFE8,) * 29)
    check("PS2 interface thunk targets",
          tuple(decoded_bodies), PS2_INTERFACE_BODIES)
    check("PS2 target operations use ordinary then unsupported cube slots",
          PS2_INTERFACE_BODIES[:2], (0x001FBE40, 0x001FBD90))
    check("PS2 unsupported cube-target diagnostic",
          b"spCubeRenderTarget not supported on PS2\0" in ps2, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}"
    )
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
