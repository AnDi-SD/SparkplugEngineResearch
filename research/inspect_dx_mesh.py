#!/usr/bin/env python3
"""Read-only regression check for PC spDXMesh and its FVF bridge."""

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
    "registration initializer": (0x006D4CB0, 0x23, "ACD2D0AB2B2634380A0B3C68FFA5547D34FCEA6A5213009B6347CDC8F10212BC"),
    "mesh-data bridge": (0x004A95F0, 0x1C, "02FE26909ABABF790868C8A8E90E1949F83B2DB477072B91E68973494CF4D16C"),
    "protected constructor thunk": (0x004A9940, 0x06, "B6F5C0AAE19C884BFBB90105AC79BB5C82CF0A3B492313DEC0F8576B8D709948"),
    "registration getter": (0x004A9990, 0x06, "356F3D0F059EA7D4B62D1F714D13D48F4D94087EDD5CBE92BFF3A623037B65E7"),
    "buffer preparation": (0x004A99A0, 0x28C, "E055BF58408809B9D4CF7A1724CDF54295C3EEE9D791F24E1814CCB3E60CC671"),
    "release interface": (0x004A9C30, 0x81, "A0848F38468867B98C9F2D6D429287F370E3C45EF415943ABED6C9742215D6A1"),
    "shared-buffer initialization": (0x004A9CC0, 0x15E, "2AC765F6212E51CE64370DD2658F543DBFE53B0BF414F5D0392E7395E26F6843"),
    "writable-buffer access": (0x004A9E20, 0x53, "44C5065091589718461FA90A1F0CF55FED8D8CE37B31A1549797288E57B2BEC2"),
    "protected factory thunk": (0x004A9E80, 0x06, "A44945D55E0D37FC1DD4009E14F988B382FA5A689F9709D07577249B509A8B1E"),
    "clone": (0x004A9EE0, 0x49, "D646E8C3D338B816947CF84E3A4CA987A37B86F430BCC8200E91166BA2A2463D"),
    "destructor": (0x004A9F50, 0xB0, "820C84F1736E1C5D2504564E28C96B4D45D89AB28F8FFD573F8A01D15E89F97C"),
    "buffer-copy interface": (0x004AA000, 0x34A, "7326A80930580620DD4DBB5277B12185ACF968FE665BDB6297994C45B11D4C66"),
    "deleting destructor": (0x004AA350, 0x1F, "1FAE532E6A162AD71F1888BADF4FA31984AE71AB0C1935E6CE8C58BDA74639FF"),
    "component flags to FVF": (0x004B21E0, 0xCD, "27D8FD129753B1AF5852F7DA857B6B23601D7C1B5BAB5CABE3350DB2C9DC15BA"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pc", type=Path, default=root / "local-data" / "pc-pristine" / "WinxClub.exe")
    parser.add_argument("--ps2", type=Path, default=root / "local-data" / "Winx Club the game PS2" / "SLES_532.19")
    args = parser.parse_args()

    pc = args.pc.read_bytes()
    ps2 = args.ps2.read_bytes()
    image_base, sections = read_pe(pc)
    read_elf(ps2)
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
    check("PC exact source path", b"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXMesh.cpp\0" in pc, True)
    check("PC class string", b"spDXMesh\0" in pc, True)
    check("PS2 class string absent", b"spDXMesh\0" in ps2, False)

    for label, (address, size, expected) in PC_BODIES.items():
        check(label, digest(image_slice(pc, sections, address - image_base, size)), expected)

    initializer = image_slice(pc, sections, 0x006D4CB0 - image_base, 0x23)
    check("class ID", struct.pack("<I", 0x193B2671) in initializer, True)
    check("direct spRenderMesh base", struct.pack("<I", 0x67974A9C) in initializer, True)
    check("protected factory address", struct.pack("<I", 0x004A9E80) in initializer, True)
    check(
        "primary vtable",
        struct.unpack("<11I", image_slice(pc, sections, 0x006EF334 - image_base, 0x2C)),
        (0x004AA350, 0x005B7A00, 0x004A9EE0, 0x00413120,
         0x004A9990, 0x00408350, 0x00408370, 0x00424360,
         0x004A95F0, 0x005A7DB0, 0x004A9CC0),
    )
    check(
        "secondary vtable",
        struct.unpack("<2I", image_slice(pc, sections, 0x006EF32C - image_base, 8)),
        (0x004A9C30, 0x004AA000),
    )

    shared = image_slice(pc, sections, 0x004A9CC0 - image_base, 0x15E)
    for offset, pattern in {
        "+0x44 component flags": "89 46 44",
        "+0x48 primitive count": "89 56 48",
        "+0x4c vertex count": "89 56 4C",
        "+0x50 index type": "89 4E 50",
        "+0x70 FVF": "89 46 70",
        "+0x74 stride": "89 56 74",
        "+0x78 index begin": "89 56 78",
        "+0x7c vertex begin": "89 56 7C",
        "+0x84 renderer code": "89 86 84 00 00 00",
    }.items():
        check(f"shared init stores {offset}", bytes.fromhex(pattern) in shared, True)

    copy = image_slice(pc, sections, 0x004AA000 - image_base, 0x34A)
    check("copy uses active combiner global",
          bytes.fromhex("A1 48 31 76 00") in copy, True)
    check("copy expands packed four bytes",
          copy.count(bytes.fromhex("DB 44 24 30")), 4)
    check("copy adds twelve bytes per vertex",
          bytes.fromhex("8D 04 88") in copy, True)
    check("copy commits into active combiner",
          bytes.fromhex("E8 A5 F6 FF FF") in copy, True)

    fvf = image_slice(pc, sections, 0x004B21E0 - image_base, 0xCD)
    check("FVF base XYZ/XYZRHW selection", bytes.fromhex("B8 02 00 00 00") in fvf
          and bytes.fromhex("B8 12 00 00 00") in fvf, True)
    check("FVF supports eight texture sets",
          all(struct.pack("<I", value) in fvf
              for value in (0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700, 0x800)), True)
    check("FVF packed field sets LASTBETA_UBYTE4",
          bytes.fromhex("0D 00 10 00 00") in fvf, True)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
