#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for the runtime camera family."""

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


PC_CAMERA_VTABLE = (
    0x00428C10, 0x00420B40, 0x004A1BF0, 0x00421F80,
    0x004288F0, 0x00408350, 0x00408370, 0x00420E30,
    0x00420E60, 0x004212F0, 0x00420610, 0x00421330,
    0x00428AF0, 0x00421640, 0x004289B0, 0x00427D40,
    0x004284B0, 0x00427CD0,
)

PC_CAMERA_DATA_VTABLE = (
    0x00435820, 0x00420B40, 0x0041AD40, 0x00421F80,
    0x00435800, 0x00408350, 0x00408370, 0x00420E30,
    0x00420E60, 0x004212F0, 0x00420610, 0x00421330,
    0x00428AF0, 0x00421640, 0x004289B0, 0x00427D40,
    0x004284B0, 0x00427CD0,
)

PC_DX_CAMERA_VTABLE = (
    0x004A91E0, 0x00420B40, 0x004A9190, 0x00421F80,
    0x004A9100, 0x00408350, 0x00408370, 0x00420E30,
    0x00420E60, 0x004212F0, 0x00420610, 0x00421330,
    0x00428AF0, 0x00421640, 0x004289B0, 0x00427D40,
    0x004284B0, 0x00427CD0,
)

PS2_CAMERA_VTABLE = (
    0, 0, 0x001B26D0, 0x001A5AC0, 0x001B2A20,
    0x001A5BE0, 0x001B0C20, 0x00100010, 0x00100050,
    0x001A87E0, 0x001A7EC0, 0x001A7D40, 0x001A7160,
    0x001A7130, 0x001B2060, 0x001A5B00, 0x001B2470,
    0x001B22F0, 0x001B0E20, 0x001B2650,
)

PS2_CAMERA_DATA_VTABLE = (
    0, 0, 0x001B2A30, 0x001A5AC0, 0x001348F0,
    0x001A5BE0, 0x00135AE0, 0x00100010, 0x00100050,
    0x001A87E0, 0x001A7EC0, 0x001A7D40, 0x001A7160,
    0x001A7130, 0x001B2060, 0x001A5B00, 0x001B2470,
    0x001B22F0, 0x001B0E20, 0x001B2650,
)

PS2_PLATFORM_CAMERA_VTABLE = (
    0, 0, 0x001F5DA0, 0x001A5AC0, 0x001F5E20,
    0x001A5BE0, 0x001F54B0, 0x00100010, 0x00100050,
    0x001A87E0, 0x001A7EC0, 0x001A7D40, 0x001A7160,
    0x001A7130, 0x001B2060, 0x001A5B00, 0x001F5D30,
    0x001F5C30, 0x001B0E20, 0x001B2650,
)

PC_BODIES = {
    "transform/view update": (0x00428AF0, 0x11E,
        "A0809E5B29F7007D7671F9D8A4ECC62F8CCE6F48144F2FAC3E1461A4EAE9F4F9"),
    "viewport configure": (0x004289B0, 0x13C,
        "C94BB78A3D8DE7CEABA78ACF59DED0C3497E721D0CB9ED13BEE34398A6877656"),
    "renderer matrix apply": (0x00427D40, 0x5A,
        "CB7E583E832A7B8ED751A0211372196918BA99A6DEECEAED2A275C279543D88E"),
    "render traversal": (0x004284B0, 0xCC,
        "FEC7C6129C8B05C2BD91AE1B446FD764D932C6B539753847F3682AD22B2BDA01"),
    "viewport teardown": (0x00427CD0, 0x4C,
        "FF4DC814ECAD98ECBA193F9402FCEF805C77AF023438218D2000F8EB72250CAB"),
    "view-angle setter": (0x00427DA0, 0x69,
        "DEA816DBC729141E7EC6297D18B24C4D29D2F6706107B8F9EB2250D8227C2B42"),
}

PS2_BODIES = {
    "common constructor": (0x001B2760, 0x2BC,
        "98DF08B90E1278628E036A56065F2E5CD03441B37978B398B3073C07CD4415C2"),
    "viewport configure": (0x001B2470, 0x1D4,
        "DEAA3CEF988E9F4AA594FE556C95FDB93C41714D786ECBC1D39DACD2E7D34656"),
    "common matrix apply": (0x001B22F0, 0x15C,
        "18141560CCEC6C4F7B5F38D8C2023DFFE543A3454B0DE335CC799D1E7DA9BF61"),
    "viewport teardown": (0x001B2650, 0x78,
        "BE6237863261910F8E4FBCE420A4FA9E58BD3A84279F383133D78966AF136791"),
    "platform matrix apply": (0x001F5C30, 0xF8,
        "2C4F1D696C5EDF82816965FF16F728EDC72443853A26A124B77EB0C407E550B5"),
    "platform viewport configure": (0x001F5D30, 0x64,
        "BA4A41BB28AD603A45AF1D73AE99703C695A3B7FAB873486752908439B8C048B"),
    "camera-data factory": (0x001349B0, 0x38,
        "1356EFD54A76C4E4C0791B15B84AB6351626648F8B427153266942104D7BFE02"),
    "platform camera factory": (0x001F5FB0, 0x8C,
        "D0F1A8FDE82BAFE308103C077A6D931EB6344CCBFFE61D3D48B9E9577F9C9F5E"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


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

    for name in ("spCamera", "spCameraData", "spDXCamera", "spCameraManager"):
        check(f"PC class string {name}", name.encode() + b"\0" in pc, True)
    for name in ("spCamera", "spCameraData", "spPS2Camera", "spCameraManager"):
        check(f"PS2 class string {name}", name.encode() + b"\0" in ps2, True)

    for text in (
        b"pDataStream->Read( fNearPlane ) ERROR: 0x%08X\0",
        b"pDataStream->Read( fPixelAspectRatio ) ERROR: 0x%08X\0",
        b"pCamera->GetViewAngle() ) ERROR: 0x%08X\0",
    ):
        check(f"PS2 serializer diagnostic {text[:24]!r}", text in ps2, True)

    for label, (address, size, expected) in PC_BODIES.items():
        check(f"PC {label}", digest(image_slice(
            pc, pc_sections, address - image_base, size)), expected)
    for label, (address, size, expected) in PS2_BODIES.items():
        check(f"PS2 {label}", digest(image_slice(
            ps2, ps2_sections, address, size)), expected)

    for label, address, expected in (
        ("PC base camera vtable", 0x006DCBC0, PC_CAMERA_VTABLE),
        ("PC camera-data vtable", 0x006DEA20, PC_CAMERA_DATA_VTABLE),
        ("PC DX camera vtable", 0x006EF1E0, PC_DX_CAMERA_VTABLE),
    ):
        check(label, struct.unpack("<18I", image_slice(
            pc, pc_sections, address - image_base, 18 * 4)), expected)
    for label, address, expected in (
        ("PS2 base camera vtable", 0x00490480, PS2_CAMERA_VTABLE),
        ("PS2 camera-data vtable", 0x0048D850, PS2_CAMERA_DATA_VTABLE),
        ("PS2 platform camera vtable", 0x00491890, PS2_PLATFORM_CAMERA_VTABLE),
    ):
        check(label, struct.unpack("<20I", image_slice(
            ps2, ps2_sections, address, 20 * 4)), expected)

    camera_factory = image_slice(ps2, ps2_sections, 0x001349B0, 0x38)
    platform_factory = image_slice(ps2, ps2_sections, 0x001F5FB0, 0x8C)
    check("PS2 camera-data allocation is 0x250",
          bytes.fromhex("50 02 04 24") in camera_factory, True)
    check("PS2 platform allocation is 0x340",
          bytes.fromhex("40 03 04 24") in platform_factory, True)
    check("PS2 platform base boundary is +0x250",
          bytes.fromhex("50 02 04 26") in platform_factory, True)
    check("PS2 platform plane-array boundary is +0x2d0",
          bytes.fromhex("D0 02 04 26") in platform_factory, True)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} "
          f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
