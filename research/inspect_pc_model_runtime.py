#!/usr/bin/env python3
"""PC-only read-only model/renderable/clone-map/SMO append static anchors."""
from pathlib import Path
import struct
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local-data/research-cache/python'))
from inspect_executable_architecture import scan_pe
from inspect_serializer_manager import read_pe, image_slice, sha256, PC_SHA256


def main():
    path = ROOT / 'local-data/pc-pristine/WinxClub.exe'
    raw = path.read_bytes()
    base, sections = read_pe(raw)
    checks = 0

    def check(value, label):
        nonlocal checks
        checks += 1
        if not value: raise AssertionError(label)

    def data(address, size): return image_slice(raw, sections, address - base, size)

    check(sha256(raw) == PC_SHA256, 'original PC executable hash')
    records = {r['class_name']: r for r in scan_pe(path, 0x12ff0)['registered_types']}
    for name, identity, parent, factory in (
            ('spModel', 0x763277db, 0x4fda4542, 0x479ed0),
            ('spRenderable', 0x4fda4542, 0x44de07fd, 0),
            ('spMeshData', 0x33c34cf0, 0x3f077b6c, 0x41a270),
            ('spCloneManager', 0xc4419f78, 0x415352a1, 0x412540),
            ('spResourceManager', 0xa4b9923b, 0x415352a1, 0x458d00)):
        record = records[name]
        check((record['class_hash'], record['base_class_hash'], record['constructor_arg5_va']) ==
              (identity, parent, factory), 'registered native identity/base/factory ' + name)
    for address, values in (
            (0x6eaa58, (0x479f90, 0x5b7a00, 0x479f40, 0x479e60, 0x479a80, 0x408350, 0x408370,
                        0x479b00, 0x423fd0, 0x479dc0, 0x4240d0, 0x479d20, 0x479d40, 0x48eaa0, 0x479da0)),
            (0x6dc9b0, (0x423fb0, 0x5b7a00, 0x4a1bf0, 0x423c70, 0x423e20, 0x408350, 0x408370,
                        0x4d6550, 0x423fd0, 0x60db76, 0x4240d0, 0x423b50, 0x423bf0, 0x423b60))):
        check(struct.unpack(f'<{len(values)}I', data(address, 4 * len(values))) == values,
              f'complete callable table{address:08X}')
    for address, target in ((0x479e69, 0x423c70), (0x469222, 0x469ed0),
                            (0x469f2f, 0x469820), (0x6d14c5, 0x52fd90),
                            (0x13c7326, 0x41d4e0)):
        code = data(address, 5)
        check(code[0] in (0xe8, 0xe9) and address + 5 + struct.unpack('<i', code[1:])[0] == target,
              f'exact call/jump{address:08X}->{target:08X}')
    for address, value, label in (
            (0x13d0a08, '6A60', 'model exact60 allocation'),
            (0x13d0a7f, 'C7465C03000000', 'PC projection group3'),
            (0x13c72c9, 'A16C527500', 'renderable constructor uses lazy DebugManager, not renderer'),
            (0x423b50, 'B8AC017600C3', 'base shared zero sphere'),
            (0x423b60, 'C7411400000000C3', 'base classifier zeros14 unlike model no-op'),
            (0x6d14c0, 'B988557500', 'isolated static clone-map initialization'),
            (0x13bf61d, '68E02B4100', 'support copy invokes always-clone412BE0'),
            (0x412beb, 'FF4614', 'clone depth increments before virtual clone'),
            (0x412c40, 'FF259C1E3B01', 'public map-aware clone protected entry'),
            (0x13bf654, '148B4E18', 'support copy continuation raw alignment')):
        check(data(address, len(bytes.fromhex(value))) == bytes.fromhex(value), label)
    print(f'PASS {checks}/{checks}: PC model/renderable/clone-map static anchors')
    return 0


if __name__ == '__main__': raise SystemExit(main())
