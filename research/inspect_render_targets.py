#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for the render-target family."""

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


PC_DX_RENDER_TARGET_INTERFACE = (
    0x004CDC70, 0x004CDBE0, 0x004CDD60, 0x004B8E80,
    0x004CDBC0, 0x005B7A00, 0x004A1BF0, 0x00413120,
    0x004CDAF0, 0x00408350, 0x00408370, 0x004CDB20,
    0x004F3DF0,
)

PC_DX_CUBE_INTERFACE = (
    0x004AC580, 0x004AC4F0, 0x004AC2A0, 0x004B8DA0,
    0x004AC670, 0x005B7A00, 0x004AC440, 0x00413120,
    0x004AC290, 0x00408350, 0x00408370, 0x004AC2F0,
    0x004F3DF0,
)

PS2_CUBE_VTABLE_GROUP = (
    0, 0, 0x00209580, 0x00100810, 0x002095F0, 0x00105DC0,
    0x00209530, 0x00100010, 0x00100050, 0, 0, 0x002097E0,
    0x002097F0, 0x002097D0, 0x002097C0, 0x001C2740,
    0x00209570, 0x00209560, 0x001C2700, 0x00209550,
    0x00209540, 0, 0, 0,
)

PS2_RENDER_TARGET_VTABLE_GROUP = (
    0, 0, 0x00209B30, 0x00100810, 0x00209BA0, 0x00105DC0,
    0x00209800, 0x00100010, 0x00100050, 0, 0, 0x00209DB0,
    0x00209DC0, 0x00209DA0, 0x00209D90, 0x001C2740,
    0x00209A20, 0x002098A0, 0x001C2700, 0x00209820,
    0x00209810, 0, 0, 0,
)

PC_BODIES = {
    "DX ordinary Init": (
        0x004CDC70, 0xE8,
        "594370265CCD728F713F5BC6580BB1B9B070A65F987B17BB40506A4EC0A90DBC"),
    "DX ordinary ReinitTargetsForDeviceReset": (
        0x004CDBE0, 0x86,
        "69672025FD526587D81A1E95908791117BE29A23196B73F8D04CA64256C1DFE6"),
    "DX ordinary ReleaseTargetsForDeviceReset": (
        0x004CDD60, 0x38,
        "87D34A54D0275067A17463CB1B95D0A93B6DF1EB74E36AE3B761E4837F61FCCC"),
    "DX cube Init": (
        0x004AC580, 0xEC,
        "D409A599C88645780CF4FAACFCB2D541539ADA217B8D6892B04315757B7086C3"),
    "DX cube ReinitTargetsForDeviceReset": (
        0x004AC4F0, 0x86,
        "E729790A3E3AA24183A44602C8A818E743AD89365F9EF75BEF63369705854BDA"),
    "DX cube ReleaseTargetsForDeviceReset": (
        0x004AC2A0, 0x47,
        "FAAD52A245BE11CFB2EA963A02E83C3343421C8934764B2DE4093BFD91ACBD14"),
    "manager release traversal": (
        0x0045CD10, 0x155,
        "EB69F045E07CD0D1075923BA4842BD64B56D14ED8839C0FF358B482BB5855771"),
    "manager reinit traversal": (
        0x0045CE70, 0x203,
        "79872F0B591D4C928089EA1C65EBFE1272883B48080520E3E780F121D4B578F6"),
}

PS2_BODIES = {
    "common target constructor window": (
        0x001C2830, 0xB0,
        "09285F58B9069B348FECF7E385DED06AA0BD6C0D4DBB990A0BD87C72896D3DE3"),
    "common cube constructor window": (
        0x001C2590, 0xC0,
        "9E37B1D0CABE957A600A5E7E3FA3001637E8CA8345F1C2400DE4A89A9FECA83E"),
    "manager constructor window": (
        0x001C3810, 0x180,
        "A20EDEDA68656B0511C4ABDE43A76F70D6BC60657E017906C9B25C70C661F91D"),
    "PS2 target Init": (
        0x002098A0, 0x180,
        "EAA662E44550A87DE6FAC2528C011B0192187D10A0432D6C228F1EEBC86DE832"),
    "PS2 target ReinitTargetsForDeviceReset": (
        0x00209A20, 0x110,
        "8EDB00B278DBA274DEFFA0D65AA7A45990F496A43DE7FB37BA8D7BA9EB05DFF4"),
    "PS2 target ReleaseTargetsForDeviceReset": (
        0x00209820, 0x80,
        "1D2E4658FEC432F74F51F172602073F9C7ED2931DFCED91D6A4529E00B8928D1"),
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

    pc_classes = (
        "spRenderTarget", "spCubeRenderTarget", "spDXRenderTarget",
        "spDXCubeRenderTarget", "spPCRenderTarget",
        "spRenderTargetManager", "spPCRenderTargetManager",
    )
    ps2_classes = (
        "spRenderTarget", "spCubeRenderTarget", "spPS2RenderTarget",
        "spPS2CubeRenderTarget", "spRenderTargetManager",
        "spPS2RenderTargetManager",
    )
    for name in pc_classes:
        check(f"PC class string {name}", name.encode() + b"\0" in pc, True)
    for name in ps2_classes:
        check(f"PS2 class string {name}", name.encode() + b"\0" in ps2, True)

    check("PC exact DX ordinary source path",
          b"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXRenderTarget.cpp\0" in pc,
          True)
    check("PC exact DX cube source path",
          b"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXCubeRenderTarget.cpp\0" in pc,
          True)
    check("PS2 exact platform source basename",
          b"spPS2RenderTarget.cpp\0" in ps2, True)
    check("PS2 exact invalid-format token",
          b"Invalid eTBPixelFormat.\0" in ps2, True)
    check("PS2 cube unsupported diagnostic",
          b"spCubeRenderTarget not supported on PS2\0" in ps2, True)

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
        "PC DX ordinary 13-slot interface",
        struct.unpack("<13I", image_slice(
            pc, pc_sections, 0x006F33F8 - image_base, 13 * 4)),
        PC_DX_RENDER_TARGET_INTERFACE)
    check(
        "PC DX cube 13-slot interface",
        struct.unpack("<13I", image_slice(
            pc, pc_sections, 0x006EF80C - image_base, 13 * 4)),
        PC_DX_CUBE_INTERFACE)
    check(
        "PS2 concrete cube vtable group",
        struct.unpack("<24I", image_slice(
            ps2, ps2_sections, 0x00491C60, 24 * 4)),
        PS2_CUBE_VTABLE_GROUP)
    check(
        "PS2 concrete ordinary vtable group",
        struct.unpack("<24I", image_slice(
            ps2, ps2_sections, 0x00491CC0, 24 * 4)),
        PS2_RENDER_TARGET_VTABLE_GROUP)

    # PC deactivation changes a node flag instead of erasing the node. Both
    # routines therefore end in the same byte store at node+0x0c.
    for label, address in (
        ("ordinary deactivation", 0x0045CBC0),
        ("cube deactivation", 0x0045CBF0),
    ):
        body = image_slice(pc, pc_sections, address - image_base, 0x25)
        check(f"PC manager {label} retains inactive node",
              bytes.fromhex("C6 40 0C 00") in body, True)

    # Direct factory constants provide the strongest PS2 sizeof boundary.
    ps2_factory = image_slice(ps2, ps2_sections, 0x00209D10, 0x50)
    cube_factory = image_slice(ps2, ps2_sections, 0x00209750, 0x50)
    manager_factory = image_slice(ps2, ps2_sections, 0x002094B0, 0x50)
    check("PS2 ordinary factory requests 0x30 bytes",
          bytes.fromhex("30 00 04 24") in ps2_factory, True)
    check("PS2 cube factory requests 0x2c bytes",
          bytes.fromhex("2C 00 04 24") in cube_factory, True)
    check("PS2 manager factory requests 0x44 bytes",
          bytes.fromhex("44 00 04 24") in manager_factory, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
