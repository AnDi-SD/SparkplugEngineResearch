#!/usr/bin/env python3
"""Read-only PC anchors for Renderable callbacks and renderer queue protocol."""
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

    check(sha256(raw) == PC_SHA256, 'pristine PC executable')
    records = {r['class_name']: r for r in scan_pe(path, 0x12ff0)['registered_types']}
    for name, identity, parent in (('spRenderer', 0x2d9c0296, 0x20a72504),
                                    ('spDXRenderer', 0x46004ee1, 0x2d9c0296),
                                    ('spPCRenderer', 0x26267c84, 0x46004ee1),
                                    ('spParticleSystem', 0x5afa1a4f, 0x4fda4542)):
        check((records[name]['class_hash'], records[name]['base_class_hash']) == (identity, parent),
              'actual native identity ' + name)
    for table in (0x6e6f78, 0x6efa40, 0x6f2918):
        check(struct.unpack('<9I', data(table + 0x20, 36)) == (0x5b7a00,) * 9,
              'all nine PC typed-pass table slots are inherited no-op, not draw bodies')
    for address, target in ((0x424011, 0x454c30), (0x424031, 0x423e30),
                            (0x424107, 0x423ea0), (0x454c74, 0x41d330),
                            (0x454c88, 0x426d40), (0x454cbf, 0x4546f0),
                            (0x456365, 0x455f70), (0x4563d9, 0x455ee0),
                            (0x456a41, 0x456090), (0x456a09, 0x455fe0)):
        code = data(address, 5)
        check(code[0] == 0xe8 and address + 5 + struct.unpack('<i', code[1:])[0] == target,
              f'actual CALL {address:08X}->{target:08X}')
    for address, expected, label in (
            (0x454d01, '3D00080000', 'alpha hard capacity2048'),
            (0x454d52, '684F1AFA5A', 'alpha flag is EXACT spParticleSystem'),
            (0x45485d, '6A18', 'qsort stride24'),
            (0x454864, 'FF15', 'qsort imported boundary, not built-in original sort'),
            (0x454876, 'C6474401', 'flush sets renderer44'),
            (0x4548aa, 'C6474400', 'flush clears renderer44'),
            (0x4548ae, 'C7474C00000000', 'flush clears alpha count'),
            (0x4240e3, 'A0FC007400', 'post restores shared global saved byte'),
            (0x42401c, '8A4654', 'pre-group enabled54'),
            (0x4240f4, '8A4655', 'post-group enabled55')):
        check(data(address, len(bytes.fromhex(expected))) == bytes.fromhex(expected), label)
    print(f'PASS {checks}/{checks}: original PC Renderable/renderer queue static anchors')
    return 0


if __name__ == '__main__': raise SystemExit(main())
