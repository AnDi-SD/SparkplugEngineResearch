#!/usr/bin/env python3
"""Read-only PC/PS2 regression check for material render-target textures."""

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


PC_MATERIAL_RT_VTABLE = (
    0x0048E610, 0x005B7A00, 0x004A1BF0, 0x0048E360,
    0x0048E490, 0x00408350, 0x00408370, 0x0048DE90,
    0x00467CA0, 0x00467CB0, 0x0060DB76, 0x0060DB76,
)

PS2_MATERIAL_RT_VTABLE = (
    0, 0, 0x00172760, 0x00100810, 0x00172D70, 0x00172470,
    0x00172460, 0x00100010, 0x00100050, 0x00172630, 0x0011BF20,
    0x0011BF30, 0, 0,
)

PS2_CAMERA_VTABLE = (
    0, 0, 0x001711C0, 0x00100810, 0x00171260, 0x00171100,
    0x00170DB0, 0x00100010, 0x00100050, 0x00172630, 0x0011BF20,
    0x0011BF30, 0x00170DC0, 0x00171150,
)

PS2_CUBE_VTABLE = (
    0, 0, 0x00171E90, 0x00100810, 0x00171F20, 0x00171DE0,
    0x001713B0, 0x00100010, 0x00100050, 0x00172630, 0x0011BF20,
    0x0011BF30, 0x00171620, 0x00171E20,
)

PC_BODIES = {
    "clear current target": (
        0x0048DE50, 0x3E,
        "A6E1CDA0BBB5B66C49F382006192318FC15E0CA5223DBAA25E40A9BF1D621131"),
    "target-or-fallback getter": (
        0x0048DE90, 0x37,
        "05CE993317F895CBA800CF391C773DEA38D25B8AB7F8446D765537BA227E03F2"),
    "target release traversal": (
        0x0048DED0, 0x3E,
        "65E3753A60C22AD0D0B104D5358D3D516CA111CC441F025402F61109623B8563"),
    "target reinit traversal": (
        0x0048DF10, 0x5C,
        "A7C2F5B5DDA9F400E0F4B738CF196787AA979E593753E92200D164843F2487F2"),
    "cube concrete factory": (
        0x004C2430, 0x5A,
        "E82726298A29F8AE9FB4D333C19A4497F40736F2ABCEDB113C6104022A42F2F8"),
}

PS2_BODIES = {
    "material-target constructor": (
        0x00172BA0, 0x1CC,
        "37817E0C8F783A6462575FDFF406E38390B58A034B87806EBCD4D14E8C2073F8"),
    "material-target copy constructor": (
        0x001729B0, 0x1D4,
        "622F0013BD3E3335BBEB6F59907C5B3160D45CA205C52589B4542EEB0B6295A1"),
    "target-or-fallback getter": (
        0x00172630, 0x90,
        "907AF5F6C33E353DDB9C32E10BAD1D8FB4E1D7F222288D9F57396655B10CB13E"),
    "write common target block": (
        0x00192900, 0x158,
        "73AF9E6709371DA70B8498CB1B0FAB4DA6DE84F3629651FD44FDB728AA8DA102"),
    "write common texture fields": (
        0x00192A60, 0x324,
        "35E0465116FD9B51077F4A351EB05CBA4A6BA8564027BDDE33F0ED587BBCC985"),
    "read common target block": (
        0x00193B10, 0x110,
        "F516D02585F3782C32C5A42D0D4B20B72391DC018686B6A1DAA3A388941927D6"),
    "read common texture fields": (
        0x00193C20, 0x1FC,
        "D999E31D293E169A5DA4BCE6EB9CD3B714901255A4E1FCCE640A4EF4DB1FE196"),
    "layer writer branches": (
        0x00192D90, 0x2C8,
        "6C364A8A804D8DFD2A4181A018C9958228D3336BAF33F3D45B45D4E50D15543F"),
    "layer reader branches": (
        0x00195300, 0x2F0,
        "494E2F69B414E1CC44177ECEA8CAB11F07B578BD7C2AE4BE60C8D73D03B24980"),
    "layer relationship index branches": (
        0x001956A0, 0x324,
        "1594D168AAA45FF1B657290A29D3C238B1D9CC9F9FA87FE0C1D24946759D8DAF"),
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

    class_names = (
        "spMaterialTexture",
        "spMaterialRenderTargetTexture",
        "spMaterialCameraViewTexture",
        "spMaterialCubeMapTexture",
    )
    for name in class_names:
        check(f"PC class string {name}", name.encode() + b"\0" in pc, True)
        check(f"PS2 class string {name}", name.encode() + b"\0" in ps2, True)

    for label, (address, size, expected) in PC_BODIES.items():
        check(
            f"PC {label}",
            digest(image_slice(pc, pc_sections, address - image_base, size)),
            expected,
        )
    for label, (address, size, expected) in PS2_BODIES.items():
        check(
            f"PS2 {label}",
            digest(image_slice(ps2, ps2_sections, address, size)),
            expected,
        )

    pc_rt_reg = image_slice(
        pc, pc_sections, 0x006D47C0 - image_base, 0x23)
    check("PC material-target class ID",
          struct.pack("<I", 0x535D1473) in pc_rt_reg, True)
    check("PC material-target direct material-texture base",
          struct.pack("<I", 0x694E6975) in pc_rt_reg, True)
    check("PC material-target null factory/callback",
          pc_rt_reg.startswith(b"\x6A\x00\x6A\x00"), True)

    for label, address, class_id, base_id, factory in (
        ("base texture", 0x006D3A90, 0x694E6975, 0x415352A1, 0x00467F30),
        ("camera leaf", 0x006D3D30, 0x34EF51B9, 0x535D1473, 0x00478090),
        ("cube leaf", 0x006D56A0, 0x1C3B499A, 0x535D1473, 0x004C2430),
    ):
        window = image_slice(pc, pc_sections, address - image_base, 0x26)
        check(f"PC {label} class ID", struct.pack("<I", class_id) in window, True)
        check(f"PC {label} base ID", struct.pack("<I", base_id) in window, True)
        check(f"PC {label} factory", struct.pack("<I", factory) in window, True)

    check(
        "PC material-target twelve-slot vtable",
        struct.unpack("<12I", image_slice(
            pc, pc_sections, 0x006EC4BC - image_base, 12 * 4)),
        PC_MATERIAL_RT_VTABLE,
    )

    check(
        "PS2 abstract material-target vtable",
        struct.unpack("<14I", image_slice(
            ps2, ps2_sections, 0x0048EA60, 14 * 4)),
        PS2_MATERIAL_RT_VTABLE,
    )
    check(
        "PS2 camera-view concrete vtable",
        struct.unpack("<14I", image_slice(
            ps2, ps2_sections, 0x0048E9A0, 14 * 4)),
        PS2_CAMERA_VTABLE,
    )
    check(
        "PS2 cube-map concrete vtable",
        struct.unpack("<14I", image_slice(
            ps2, ps2_sections, 0x0048E9E0, 14 * 4)),
        PS2_CUBE_VTABLE,
    )

    for label, address, allocation in (
        ("material texture", 0x00173630, 0x80),
        ("camera-view texture", 0x00171340, 0xB0),
        ("cube-map texture", 0x00172010, 0xB0),
    ):
        factory = image_slice(ps2, ps2_sections, address, 0x60)
        check(f"PS2 {label} factory allocation",
              struct.pack("<I", 0x24040000 | allocation) in factory, True)

    # These serializer class tests distinguish layer wrappers from the nested
    # texture objects. In particular, spCubeEnvMapLayer takes the ordinary
    # path, while spMirrorLayer owns the dynamic cube-map branch.
    layer_writer = image_slice(ps2, ps2_sections, 0x00192D90, 0x2C8)
    for name, class_id in (
        ("standard", 0x234C576B),
        ("environment", 0x427C7480),
        ("cube environment", 0x4DED3E44),
        ("camera view", 0x194613E1),
        ("mirror", 0x46B61C67),
        ("movie", 0x075F3EB6),
    ):
        # MIPS constructs constants from two immediates, so check each half.
        upper = (class_id >> 16) & 0xFFFF
        lower = class_id & 0xFFFF
        check(f"PS2 writer recognizes {name} layer",
              struct.pack("<H", upper) in layer_writer
              and struct.pack("<H", lower) in layer_writer, True)

    for token in (
        b"esfMaterialLayerRenderTarget",
        b"esfMaterialLayerCamera",
        b"esfMaterialLayerCubeMap",
        b"esfMaterialLayerMovie",
        b"GetFallBackTexture()",
        b"GetNumFacesToRenderPerTick()",
    ):
        check(f"PS2 diagnostic token {token.decode()}", token in ps2, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
