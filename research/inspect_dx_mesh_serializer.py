#!/usr/bin/env python3
"""Read-only regression check for the PC-only spDXMeshSerializer."""

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
    "registration initializer": (0x006D51C0, 0x23, "9DF12D7F25943A1A74EE5E314E492E6C2495CB40A4A79F5AD9A0BD07E2025A57"),
    "registration getter": (0x004B1660, 0x06, "517162EC76E2C239273232B2B0A74132556122E2C27DE9B84F175FFCC617E345"),
    "destructor": (0x004B1670, 0x12, "C60C840AFD01DE809832EB319B6A039D4E7421A9D4B83AD796A2BE94D8E32E2E"),
    "target class getter": (0x004B1690, 0x06, "5A76A159CEDFA7682409A3F76553E495B84C54A8CEA237BBBE570B4AF9C3E052"),
    "protected factory thunk": (0x004B16A0, 0x06, "2283CA3384F5CF14FE2236ADB9C4CBEF4679D4BE699AFE04A02765AB0EE54E1E"),
    "clone": (0x004B1710, 0x49, "8ECAB0057B573BB9669341B4C8AAE3ECEA2502C21621729D98EAE581E9229449"),
    "deleting destructor": (0x004B1760, 0x1F, "40D0B2EF4B42913E2B4ED710000FC16102AC872CA9663CAA53AF400BB29DCA14"),
    "read": (0x004B1780, 0x394, "9B8E4315D17E189FF0318DFBFFFE53A0A5290042C7D0FC1B4011FCFED17954DA"),
    "write": (0x004B1B20, 0x28C, "399C08365D3D322C8690048F508DEB61D0D2703F76CB0262CC5C2555634277D0"),
    "index relationship": (0x004B1DB0, 0x50, "1C010F905857A961EDDE1A463B5D692F9273A0137A1DFE683A591D21F414B2C4"),
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
    check("PC class string", b"spDXMeshSerializer\0" in pc, True)
    check("PS2 class string absent", b"spDXMeshSerializer\0" in ps2, False)

    for label, (address, size, expected) in PC_BODIES.items():
        check(label, digest(image_slice(pc, sections, address - image_base, size)), expected)

    initializer = image_slice(pc, sections, 0x006D51C0 - image_base, 0x23)
    check("class ID", struct.pack("<I", 0xE712BCAD) in initializer, True)
    check("direct spSerializer base", struct.pack("<I", 0x42429877) in initializer, True)
    check("protected factory address", struct.pack("<I", 0x004B16A0) in initializer, True)
    check("registration address", struct.pack("<I", 0x00763BA0) in initializer, True)
    check(
        "primary vtable",
        struct.unpack("<11I", image_slice(pc, sections, 0x006F0034 - image_base, 0x2C)),
        (0x004B1760, 0x005B7A00, 0x004B1710, 0x0040ECE0,
         0x004B1660, 0x00408350, 0x00408370, 0x00467550,
         0x004671E0, 0x004672C0, 0x004B1690),
    )
    check(
        "secondary vtable",
        struct.unpack("<3I", image_slice(pc, sections, 0x006F0028 - image_base, 0x0C)),
        (0x004B1B20, 0x004B1DB0, 0x004B1780),
    )

    read = image_slice(pc, sections, 0x004B1780 - image_base, 0x394)
    check("read requests shared-mesh-data relationship",
          bytes.fromhex("68 81 26 3A 29") in read, True)
    check("read calls shared-buffer mesh initializer",
          bytes.fromhex("FF 50 28") in read, True)
    read_labels = (
        b"eIndexType", b"uVertexComponents", b"uFVFCode",
        b"uIndexBegin", b"uVertexBegin", b"uIndexCount",
        b"uVertexCount", b"uVertexStructSize", b"vCenter", b"fRadius",
    )
    check("read diagnostics preserve all field names",
          all(label in pc for label in read_labels), True)

    write = image_slice(pc, sections, 0x004B1B20 - image_base, 0x28C)
    check("write reads mesh type at +0x50", bytes.fromhex("8B 47 50") in write, True)
    check("write reads component flags at +0x44", bytes.fromhex("8B 47 44") in write, True)
    check("write reads FVF at +0x70", bytes.fromhex("8B 47 70") in write, True)
    check("write acquires renderer-owned relationship",
          bytes.fromhex("E8 14 D2 00 00") in write, True)
    check("write asks relationship for five range values",
          bytes.fromhex("E8 9A EB 00 00") in write, True)
    check("write serializes center at +0x18", bytes.fromhex("8D 4F 18") in write, True)
    check("write serializes radius at +0x24", bytes.fromhex("8B 47 24") in write, True)
    check("relationship diagnostic", b"SerializeRelationship(pDataStream, pVertexBuffer)" in pc, True)
    check("index diagnostic", b"IndexRelationship(pVertexBuffer)" in pc, True)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
