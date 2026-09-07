#!/usr/bin/env python3
"""Read-only PC registration/call/ABI anchors for the scene-world checkpoint."""
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
        if not condition:
            raise AssertionError(label)

    def data(address, size):
        return image_slice(raw, sections, address - base, size)

    check(sha256(raw) == PC_SHA256, 'pristine original image SHA256')
    records = {r['class_name']: r for r in scan_pe(path, 0x12ff0)['registered_types']}
    for name, class_id, parent, factory in (
        ('spSceneManager', 0x67419388, 0x415352a1, 0x45adf0),
        ('spScene', 0x6f927c11, 0x44de07fd, 0x45ebc0),
        ('spPCLensFlareManager', 0x4838786b, 0x782e7d46, 0x4c7240),
        ('spPCProjectionManager', 0xe10d6fc2, 0x24a010da, 0x4c5ce0),
        ('spSkyBoxManager', 0x61c23595, 0x415352a1, 0x48dc80),
        ('spLightManager', 0x6fcd243a, 0x415352a1, 0x46ab80),
        ('spDXCamera', 0x41672e34, 0x18df3845, 0x4a9120),
        ('spCameraData', 0x24bb4c41, 0x18df3845, 0x41a3f0)):
        record = records[name]
        check((record['class_hash'], record['base_class_hash'], record['constructor_arg5_va']) ==
              (class_id, parent, factory), f'{name} exact registry identity/factory')
    for address, targets in (
        (0x6e7154, (0x45add0, 0x5b7a00, 0x45ae50, 0x40ece0, 0x45acc0, 0x408350, 0x408370)),
        (0x6e7358, (0x45eb50, 0x5b7a00, 0x45ec20, 0x413120, 0x45e720, 0x408350, 0x408370))):
        check(struct.unpack('<7I', data(address, 28)) == targets, 'scene family seven-slot native vtable')
    for call, target in ((0x41cdf3, 0x45a7d0), (0x41c303, 0x45ebc0), (0x41c30d, 0x45d850),
                         (0x41c36d, 0x4a9120), (0x45eaae, 0x421e20), (0x45eaf8, 0x45a970),
                         (0x45eb17, 0x4c7240), (0x45eb1f, 0x4c5ce0), (0x45eb27, 0x48dc80),
                         (0x45eb2f, 0x46ab80), (0x45e61b, 0x45ab30), (0x421b57, 0x45ace0),
                         (0x428574, 0x45ec70)):
        code = data(call, 5)
        check(code[0] == 0xe8 and call + 5 + struct.unpack('<i', code[1:])[0] == target,
              f'original CALL {call:08X}->{target:08X}')
    check(data(0x45a7eb, 7) == bytes.fromhex('6A 00 8B C8 FF 52 30'), 'scene root virtual30 with zero argument')
    check(data(0x427d6d, 6) == bytes.fromhex('8D 96 CC 00 00 00'), 'camera view pointer CC')
    check(data(0x427d89, 6) == bytes.fromhex('81 C6 0C 01 00 00'), 'camera projection pointer10C')
    check(data(0x427dbf, 7) == bytes.fromhex('C6 81 C8 00 00 00 00'), 'angle setter clears2D flag')
    check(data(0x41c3d8, 3) == bytes.fromhex('FF 40 38') and
          data(0x41c3f5, 3) == bytes.fromhex('FF 40 38'), 'both timer append branches increment childCount38')
    print(f'PASS {checks}/{checks}: PC scene/core/timer/camera static anchors')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
