#!/usr/bin/env python3
"""Read-only regression check for spDXMeshCombiner in WinxClub.exe."""

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
    x86_call_count,
)


PC_BODIES = {
    "IsFull": (
        0x004A95E0, 0x0A,
        "94A8C99FEB700D7ED6C654A1ED6070709A8DFEC64BF1DA07B3C7692738F52E9A"),
    "constructor": (
        0x004A9610, 0x26,
        "A4E97075DBC7312842D9933B6C5CDE7C1AB2C5F685F7F684C49325EE4207AC32"),
    "destructor": (
        0x004A9640, 0x73,
        "B268299C17FF7F6DE8AD93DE77429718E7D66D067750290F0D183713E851F71A"),
    "Initialize": (
        0x004A96C0, 0x220,
        "D1208BB6603C6A0095618EB22B45E2AD9803DF0D29A6991E8B7581E8286E7817"),
    "Commit": (
        0x004A98E0, 0x54,
        "8FD6CDF2A6111A01F25BFD940926A13504929F3360AF9C313445B63C8445A3D5"),
    "deleting destructor": (
        0x004A9F30, 0x1E,
        "0860D4D6B4F1C610DD43D4656212B968F5A1FA93FD9C68C5D197D7311036871E"),
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
    check("PC diagnostic class name", b"spDXMeshCombiner\0" in pc, True)
    check("PS2 diagnostic class name absent", b"spDXMeshCombiner\0" in ps2, False)
    check(
        "exact source path",
        b"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXMesh.cpp\0" in pc,
        True,
    )

    for label, (address, size, expected) in PC_BODIES.items():
        body = image_slice(pc, pc_sections, address - image_base, size)
        check(f"PC {label}", digest(body), expected)

    check(
        "single-entry vtable",
        struct.unpack("<I", image_slice(
            pc, pc_sections, 0x006EF294 - image_base, 4))[0],
        0x004A9F30,
    )
    check("constructor direct callers",
          x86_call_count(pc, pc_sections, 0x004A9610 - image_base), 1)
    check("Initialize direct callers",
          x86_call_count(pc, pc_sections, 0x004A96C0 - image_base), 1)
    check("IsFull direct callers",
          x86_call_count(pc, pc_sections, 0x004A95E0 - image_base), 1)
    check("Commit direct callers",
          x86_call_count(pc, pc_sections, 0x004A98E0 - image_base), 1)

    constructor = image_slice(
        pc, pc_sections, 0x004A9610 - image_base, 0x26)
    check("constructor installs vtable",
          bytes.fromhex("C7 00 94 F2 6E 00") in constructor, True)
    check("constructor leaves +0x04 untouched",
          b"\x89\x48\x04" in constructor, False)
    for offset in range(0x08, 0x2C, 4):
        check(
            f"constructor zeros +0x{offset:02X}",
            bytes((0x89, 0x48, offset)) in constructor,
            True,
        )

    initialize = image_slice(
        pc, pc_sections, 0x004A96C0 - image_base, 0x220)
    check("Initialize stores FVF +0x0C",
          b"\x89\x46\x0C" in initialize, True)
    check("Initialize stores target vertices +0x04",
          b"\x89\x4E\x04" in initialize, True)
    check("Initialize stores vertex bytes +0x10",
          b"\x89\x56\x10" in initialize, True)
    check("Initialize stores index bytes +0x14",
          b"\x89\x5E\x14" in initialize, True)
    check("Initialize locks vertex buffer",
          b"\xFF\x51\x2C" in initialize, True)
    check("Initialize locks index buffer",
          initialize.count(b"\xFF\x51\x2C"), 2)

    commit = image_slice(pc, pc_sections, 0x004A98E0 - image_base, 0x54)
    check("Commit advances written vertices",
          b"\x89\x4E\x08" in commit, True)
    check("Commit advances written indices",
          b"\x89\x56\x28" in commit, True)
    check("Commit advances vertex cursor",
          b"\x89\x4E\x20" in commit, True)
    check("Commit advances index cursor",
          b"\x89\x56\x24" in commit, True)
    check("Commit unlocks both D3D buffers",
          commit.count(b"\xFF\x52\x30")
            + commit.count(b"\xFF\x51\x30"), 2)

    hook = image_slice(pc, pc_sections, 0x004AA9A0 - image_base, 0x176)
    check("hook allocates exact 0x2C helper",
          b"\x6A\x2C" in hook, True)
    check("hook publishes active helper global",
          bytes.fromhex("89 35 48 31 76 00") in hook, True)
    check("hook clears active helper global",
          bytes.fromhex("C7 05 48 31 76 00 00 00 00 00") in hook, True)

    print(
        f"RESULT {'PASS' if not failures else 'FAIL'} "
        f"checks={checks - len(failures)}/{checks}"
    )
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
