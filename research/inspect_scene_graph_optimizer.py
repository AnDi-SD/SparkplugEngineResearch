#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for scene-graph optimizer evidence."""

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
    "common registration initializer": (
        0x006D5640, 0x23,
        "23B3CCD320654A4E8AB6FF9B66DD0BD984AFF4BFA42ED2A1D338476772CA6A43"),
    "DX registration initializer": (
        0x006D5520, 0x26,
        "D47974467AF0CAF07C054097B46633F079901E1610705EA10C8008FC1CEBC1ED"),
    "Optimize": (
        0x004C1900, 0xD0,
        "BB9CCAEA0D839B27BC479218509BFD5E70B529D5D143B91BDA52C6F47CF5C6B5"),
    "OptimizeNode": (
        0x004C19D0, 0x23E,
        "D3E94E65268EE322C2C5BB3C46AAAAE3E305BDCBDCB611901D190D4E68B905C7"),
    "common deleting destructor": (
        0x004C1D80, 0x1E,
        "A6416317EA1C043CDC6092ADB0086E03CFA052D84C55B7484BCD5ED812293C48"),
    "DX OnNode": (
        0x004BEEE0, 0xD3,
        "E56D519E16C8BBE664C31CA0B8412D008D00797C19BFB8FF93D29E89F3F57CC1"),
    "DX OnEndOptimize": (
        0x004BFA10, 0x0D,
        "27C741CD7A34D9C5F0B9F37A1A90BE66858684162836F8FA73C78503829BA62E"),
    "DX combined-VB lookup protected entry": (
        0x004BEDF0, 0x06,
        "1FDFE95A9E4F34D4574EA74DE7A2F56458DE2E61F95F77C84FFDC63DB2F777BD"),
    "DX OnEnd protected body entry": (
        0x004BF9A0, 0x06,
        "49F7D99344FBEC8820180283FA7360AD768176396B5870353755533D4E0A5D02"),
    "DX destructor protected body entry": (
        0x004BFD90, 0x06,
        "98138C866E1483F8C0618535339AA802580B74EEDE30703744B6E3A7D646069D"),
    "DX add-mesh protected entry": (
        0x004BFF70, 0x06,
        "CD99A15D4DBCF93A26828C2084271540900D5FD0BE7C816DBE8F9DAFFA02F2DF"),
    "DX model-mesh forwarding callback": (
        0x004C0100, 0x13,
        "6B4A363A420BBAC8D168E7BBCC73BE1F9294A0584E5BA37F0FFB18F208C676AE"),
    "DX CombineData protected entry": (
        0x004C0540, 0x06,
        "F37764C66670AE0E4504BB56595742B6829298008C57DA7FF8229C4ED86DFA52"),
    "DX combined-VB range lookup protected entry": (
        0x004C07E0, 0x06,
        "2E3DE6B709B29412A1E65DAC80DA1F699699E78440E80D869F3A558B479B9250"),
}

PS2_BODIES = {
    "common registration initializer": (
        0x00482F10, 0x34,
        "ACE519F938545AFE4A76196426C06A32699F147E44681046150D7280C2564EEE"),
}


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
    check("PC common class string", b"spSceneGraphOptimizer\0" in pc, True)
    check("PS2 common class string", b"spSceneGraphOptimizer\0" in ps2, True)
    check("PC DX class string", b"spDXSceneGraphOptimizer\0" in pc, True)
    check("PS2 has no DX optimizer leaf", b"spDXSceneGraphOptimizer\0" in ps2, False)

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

    common_initializer = image_slice(
        pc, pc_sections, 0x006D5640 - image_base, 0x23)
    check("PC common class ID",
          struct.pack("<I", 0x4FE639C2) in common_initializer, True)
    check("PC common direct spCrossPlatform base",
          struct.pack("<I", 0x20A72504) in common_initializer, True)
    check("PC common registration object",
          struct.pack("<I", 0x00764588) in common_initializer, True)
    check("PC common null factory",
          common_initializer.startswith(bytes.fromhex("6A 00 6A 00")), True)

    dx_initializer = image_slice(
        pc, pc_sections, 0x006D5520 - image_base, 0x26)
    check("PC DX class ID",
          struct.pack("<I", 0x7E120EC3) in dx_initializer, True)
    check("PC DX direct common base registration",
          struct.pack("<I", 0x00764588) in dx_initializer, True)
    check("PC DX protected factory",
          struct.pack("<I", 0x004C0050) in dx_initializer, True)
    check("PC DX registration object",
          struct.pack("<I", 0x007643A8) in dx_initializer, True)

    check(
        "PC common primary vtable",
        struct.unpack(
            "<10I", image_slice(
                pc, pc_sections, 0x006F20BC - image_base, 0x28)),
        (0x004C1D80, 0x005B7A00, 0x004A1BF0, 0x00413120,
         0x004C1CC0, 0x00408350, 0x00408370, 0x004C1900,
         0x004C19D0, 0x005A7DB0),
    )
    check(
        "PC common singleton vtable",
        struct.unpack(
            "<I", image_slice(
                pc, pc_sections, 0x006F20B8 - image_base, 4)),
        (0x004C1CD0,),
    )
    check(
        "PC DX primary vtable",
        struct.unpack(
            "<10I", image_slice(
                pc, pc_sections, 0x006F1E2C - image_base, 0x28)),
        (0x004BFF50, 0x005B7A00, 0x004C00B0, 0x00413120,
         0x004BFE70, 0x00408350, 0x00408370, 0x004C1900,
         0x004C19D0, 0x005A7DB0),
    )
    check(
        "PC DX callback vtable",
        struct.unpack(
            "<5I", image_slice(
                pc, pc_sections, 0x006F1E14 - image_base, 0x14)),
        (0x004F3DF0, 0x004BFA10, 0x004BEEE0, 0x005A7DB0,
         0x004C0100),
    )
    check(
        "PC DX singleton vtable",
        struct.unpack(
            "<I", image_slice(
                pc, pc_sections, 0x006F1E28 - image_base, 4)),
        (0x004BFE80,),
    )

    optimize = image_slice(pc, pc_sections, 0x004C1900 - image_base, 0xD0)
    optimize_node = image_slice(
        pc, pc_sections, 0x004C19D0 - image_base, 0x23E)
    check("PC Optimize resets temporary vector +0x24",
          bytes.fromhex("89 6E 24 89 6E 28 89 6E 2C") in optimize, True)
    check("PC Optimize calls callback subobject +0x18",
          bytes.fromhex("8D 5E 18") in optimize, True)
    check("PC Optimize invokes virtual OptimizeNode slot +0x20",
          bytes.fromhex("FF 52 20") in optimize, True)
    common_destructor = image_slice(
        pc, pc_sections, 0x004C1C17 - image_base, 0xA9)
    check("PC destructor restores callback vptr +0x18",
          bytes.fromhex("C7 46 18 40 1F 6F 00") in common_destructor, True)
    check("PC destructor restores support vptr +0x1c",
          bytes.fromhex("C7 46 1C 40 30 70 00") in common_destructor, True)
    check("PC destructor destroys detach list +0x30",
          bytes.fromhex("8D 7E 30") in common_destructor, True)
    check("PC destructor frees temporary begin +0x24",
          bytes.fromhex("8B 46 24") in common_destructor, True)
    check("PC traversal recognizes spRenderNode",
          struct.pack("<I", 0x603625D0) in optimize_node, True)
    check("PC traversal recognizes spModel",
          struct.pack("<I", 0x763277DB) in optimize_node, True)
    check("PC traversal reads renderable begin +0xbc",
          bytes.fromhex("8B 8E BC 00 00 00") in optimize_node, True)
    check("PC traversal reads renderable end +0xc0",
          bytes.fromhex("8B 86 C0 00 00 00") in optimize_node, True)
    check("PC diagnostics name all four callbacks",
          all(text in pc for text in (
              b"OnStartOptimize(pNode) ERROR",
              b"OnEndOptimize() ERROR",
              b"OnNode(pNode) ERROR",
              b"OnModel( pRenderNode")), True)

    dx_on_node = image_slice(
        pc, pc_sections, 0x004BEEE0 - image_base, 0xD3)
    check("PC DX OnNode reads group-map sentinel at callback +0x34",
          bytes.fromhex("8B 45 34") in dx_on_node, True)
    check("PC DX OnNode reads batch-list sentinel at callback +0x28",
          bytes.fromhex("8B 45 28") in dx_on_node, True)
    check("PC DX callback forwards spModel base mesh +0x58",
          image_slice(pc, pc_sections, 0x004C0100 - image_base, 0x13)
          == bytes.fromhex(
              "8B 44 24 08 8B 40 58 50 83 C1 E8 E8 60 FE FF FF C2 08 00"),
          True)

    mesh_writer = image_slice(
        pc, pc_sections, 0x004B1B20 - image_base, 0x28C)
    check("PC mesh writer calls DX combined-VB lookup",
          bytes.fromhex("E8 14 D2 00 00") in mesh_writer, True)
    check("PC mesh writer calls combined-VB five-range lookup",
          bytes.fromhex("E8 9A EB 00 00") in mesh_writer, True)

    ps2_initializer = image_slice(ps2, ps2_sections, 0x00482F10, 0x34)
    check("PS2 common class ID halves",
          all(pattern in ps2_initializer for pattern in (
              bytes.fromhex("E6 4F 03 3C"),
              bytes.fromhex("C2 39 65 34"))), True)
    check("PS2 common direct spCrossPlatform base halves",
          all(pattern in ps2_initializer for pattern in (
              bytes.fromhex("A7 20 02 3C"),
              bytes.fromhex("04 25 46 34"))), True)
    check("PS2 common null factory/callback",
          ps2_initializer.endswith(bytes.fromhex("2D 48 00 00 6C 45 04 08 2D 50 00 00")),
          True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
