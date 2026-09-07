#!/usr/bin/env python3
"""Read-only PC regression check for native spTransformTrackEval evidence."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from inspect_serializer_manager import PC_SHA256, image_slice, read_pe, sha256


VTABLE = (
    0x005FF070, 0x005B7A00, 0x005FF0F0, 0x0040ECE0,
    0x005FEBA0, 0x00408350, 0x00408370, 0x004D6550, 0x005FEBB0,
)

BODIES = {
    "destructor": (0x005FEB90, 0x0B,
        "2497AEEAA5B7FF2376660BD658BB4DC0883D010868A8FDAEA36E819F38B9A46B"),
    "PRS evaluator": (0x005FEBB0, 0x44A,
        "D229261370749A9FEF863C9915AEAE67FF8A8D2A007FCD5A5C16826C661F1F67"),
    "protected constructor": (0x005FF000, 0x62,
        "4CC9FF20D924C84D47F9F6F5FAD9CE09D9799979389C6A550EFF49740A7BD998"),
    "protected factory": (0x005FF090, 0x57,
        "98202313C809F0A646D73312E661045C9B41D7230D9B57AB8D37ADEFC0BE830B"),
    "clone": (0x005FF0F0, 0x49,
        "851FBEFDB1C65C37DACA9F080680A098EC71D8712A710AEF1BA201CCA1686CB8"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pc", type=Path,
        default=root / "local-data" / "pc-pristine" / "WinxClub.exe")
    args = parser.parse_args()

    executable = args.pc.read_bytes()
    image_base, sections = read_pe(executable)
    checks = 0
    failures: list[str] = []

    def pc_slice(address: int, size: int) -> bytes:
        return image_slice(executable, sections, address - image_base, size)

    def check(label: str, actual: object, expected: object) -> None:
        nonlocal checks
        checks += 1
        print(f"{'OK' if actual == expected else 'FAIL':4} {label}: {actual!r}")
        if actual != expected:
            failures.append(f"{label}: expected {expected!r}, got {actual!r}")

    check("PC SHA-256", sha256(executable), PC_SHA256)
    for label, (address, size, expected) in BODIES.items():
        check(label, digest(pc_slice(address, size)), expected)

    check("complete nine-slot vtable", struct.unpack("<9I", pc_slice(0x00711404, 36)), VTABLE)
    check("vtable ends at class string", pc_slice(0x00711428, 21), b"spTransformTrackEval\x00")
    check("registration getter", pc_slice(0x005FEBA0, 6),
          bytes.fromhex("B8 90 8E 76 00 C3"))
    factory = pc_slice(0x005FF090, 0x57)
    check("factory allocates 0x78 bytes",
          bytes.fromhex("6A 78") in factory, True)
    constructor = pc_slice(0x005FF000, 0x62)
    check("binding slot starts at -1",
          bytes.fromhex("C7 46 10 FF FF FF FF") in constructor, True)
    check("two 0x30-byte blend inputs",
          bytes.fromhex("B9 02 00 00 00") in constructor
          and bytes.fromhex("83 C0 30") in constructor, True)
    evaluator = pc_slice(0x005FEBB0, 0x44A)
    check("blend input count comes from +0x14",
          bytes.fromhex("8B 4A 14") in evaluator, True)
    check("blend input loop advances by 0x30",
          bytes.fromhex("83 C6 30") in evaluator, True)
    check("registration class string",
          b"spTransformTrackEval\x00" in executable, True)

    print(f"RESULT {'PASS' if not failures else 'FAIL'} "
          f"checks={checks - len(failures)}/{checks}")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
