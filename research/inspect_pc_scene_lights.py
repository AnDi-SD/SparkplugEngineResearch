#!/usr/bin/env python3
"""Read-only PC light family/manager/selection static anchors."""
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
    def check(condition, label):
        nonlocal checks
        checks += 1
        if not condition: raise AssertionError(label)
    def data(address, size): return image_slice(raw, sections, address - base, size)
    check(sha256(raw) == PC_SHA256, 'pristine original PC hash')
    records = {r['class_name']: r for r in scan_pe(path, 0x12ff0)['registered_types']}
    for name, identity, parent, factory in (
        ('spLightManager', 0x6fcd243a, 0x415352a1, 0x46ab80),
        ('spLight', 0x72444900, 0x695c0f65, 0),
        ('spLightData', 0x5e6402df, 0x72444900, 0x41a330)):
        record = records[name]
        check((record['class_hash'], record['base_class_hash'], record['constructor_arg5_va']) ==
              (identity, parent, factory), name + ' original registration identity')
    check(struct.unpack('<7I', data(0x6e8ca4, 28)) ==
          (0x46a830, 0x5b7a00, 0x46abf0, 0x40ece0, 0x46a7d0, 0x408350, 0x408370),
          'LightManager primary7 slots and inherited base copy')
    check(struct.unpack('<15I', data(0x6de990, 60)) ==
          (0x435430, 0x420b40, 0x41aca0, 0x428eb0, 0x435400, 0x408350, 0x408370,
           0x420e30, 0x420e60, 0x4212f0, 0x420610, 0x421330, 0x428c30, 0x421640, 0x428dd0),
          'LightData primary15 includes light world and debug helper')
    for address, target in ((0x428c39, 0x421420), (0x428c61, 0x46ace0),
                            (0x46aa54, 0x46a850), (0x46aa64, 0x490b50),
                            (0x46ac76, 0x46a850), (0x46ac86, 0x490b50),
                            (0x46ac8d, 0x490ba0), (0x46acd0, 0x46ab20),
                            (0x46ab37, 0x46a950), (0x46ab44, 0x490b50),
                            (0x46ab4b, 0x490ba0), (0x46a7f9, 0x490ba0),
                            (0x420e14, 0x420de0)):
        code = data(address, 5)
        check(code[0] == 0xe8 and address + 5 + struct.unpack('<i', code[1:])[0] == target,
              f'original CALL{address:08X}->{target:08X}')
    check(data(0x46a86b, 6) == bytes.fromhex('C1 E9 08 F6 C1 01'), 'selection uses hierarchy bit100, not200')
    check(data(0x46a87b, 6) == bytes.fromhex('8A 8A 21 01 00 00'), 'node121 controls shadow-light exclusion')
    check(data(0x46ac46, 12) == bytes.fromhex('89 90 14 01 00 00 89 90 10 01 00 00'),
          'cache rebuild resets count114/ambient110 only')
    print(f'PASS {checks}/{checks}: PC scene-light static anchors')
    return 0


if __name__ == '__main__': raise SystemExit(main())
