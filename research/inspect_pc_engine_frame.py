#!/usr/bin/env python3
"""Read-only hash/identity/call anchors for the PC engine-frame/timer checkpoint."""
from pathlib import Path
import struct
from inspect_serializer_manager import PC_SHA256, sha256, read_pe, image_slice
from inspect_executable_architecture import scan_pe

ROOT = Path(__file__).resolve().parents[1]
checks = 0


def check(condition, label):
    global checks
    checks += 1
    if not condition:
        raise AssertionError(label)


def main():
    path = ROOT / 'local-data/pc-pristine/WinxClub.exe'
    raw = path.read_bytes()
    check(sha256(raw) == PC_SHA256, 'pristine PC SHA256')
    base, sections = read_pe(raw)
    def data(address, size):
        return image_slice(raw, sections, address - base, size)
    def call(address, target):
        code = data(address, 5)
        check(code[0] == 0xe8 and address + 5 + struct.unpack_from('<i', code, 1)[0] == target,
              f'CALL{address:08X}->{target:08X}')
    for address, target in ((0x4c2d7d, 0x41cd50), (0x4c2dbc, 0x41c460),
                            (0x41cdd1, 0x4535a0), (0x41b3b3, 0x454640),
                            (0x41b3bb, 0x4533a0), (0x41b3c3, 0x4c4200),
                            (0x41b3cb, 0x452380), (0x41b3d3, 0x4512d0)):
        call(address, target)
    check(data(0x41cdce, 3) == bytes.fromhex('8b4e3c'), 'animation manager receiver from engine3C')
    check(data(0x41c554, 9) == bytes.fromhex('68b00645006a026a3c'),
          'core dtor destroys two3C task timers at54')
    check(data(0x41c55d, 3) == bytes.fromhex('8d4e54'), 'inline task timer base offset54')
    table = struct.unpack('<11I', data(0x6e6968, 44))
    check(table == (0x450730, 0x5b7a00, 0x450910, 0x40ece0, 0x4506c0,
                    0x408350, 0x408370, 0x450750, 0x450840, 0x4506d0, 0x4506e0),
          'full task timer vtable incl four local operations')
    check(struct.unpack('<I', data(0x6e6994, 4))[0] == 0x3a83126f, 'original float32 milliseconds multiplier')
    catalog = scan_pe(path, 0x12ff0)
    by_factory = {r['constructor_arg5_va']: r for r in catalog['registered_types'] if r['constructor_arg5_va']}
    expected = ((0x4533a0, 'spCinematicManager', 0x48e66610),
                (0x4c4200, 'spDXAudioManager', 0x116f7e16),
                (0x452380, 'spGUIManager', 0x73634c2d),
                (0x4512d0, 'spNetworkManager', 0x0546dec1),
                (0x459f00, 'spPhysicsManager', 0x436bff01),
                (0x45a530, 'spParticleSystemManager', 0x00921a65),
                (0x45adf0, 'spSceneManager', 0x67419388),
                (0x4c4970, 'spPCRenderTargetManager', 0x165c006f),
                (0x450880, 'spTaskTimer', 0x1acd36e2))
    for factory, name, class_id in expected:
        record = by_factory[factory]
        check((record['class_name'], record['class_hash']) == (name, class_id),
              f'{factory:08X} canonical factory identity')
    print(f'PASS {checks}/{checks}: original PC frame/timer call and registration anchors')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
