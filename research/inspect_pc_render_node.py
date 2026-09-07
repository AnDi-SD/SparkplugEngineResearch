#!/usr/bin/env python3
"""Read-only PC-only anchors for render-node layout/world/cull/dispatch."""
from pathlib import Path
import hashlib
import struct
from inspect_render_node import PC_BODIES
from inspect_serializer_manager import read_pe, image_slice, sha256, PC_SHA256


def main():
    raw = (Path(__file__).resolve().parents[1] / 'local-data/pc-pristine/WinxClub.exe').read_bytes()
    base, sections = read_pe(raw)
    checks = 0

    def check(condition, label):
        nonlocal checks
        checks += 1
        if not condition: raise AssertionError(label)

    def data(address, size):
        return image_slice(raw, sections, address - base, size)

    check(sha256(raw) == PC_SHA256, 'pristine PC SHA256')
    for label, (address, size, digest) in PC_BODIES.items():
        check(hashlib.sha256(data(address, size)).hexdigest().upper() == digest, label)
    for address, targets in (
        (0x6dcaa4, (0x4255d0, 0x420b40, 0x425580, 0x424980, 0x425030, 0x408350, 0x408370,
                    0x424760, 0x4249f0, 0x424af0, 0x420610, 0x421330, 0x4250f0, 0x424e70)),
        (0x6dcadc, (0x424b60, 0x4248d0, 0x424c30, 0x425040, 0x424790, 0x4247b0))):
        check(struct.unpack(f'<{len(targets)}I', data(address, 4 * len(targets))) == targets,
              'primary14 and adjusted support6 are separate tables')
    check(data(0x425040, 6) == bytes.fromhex('81 E9 B4 00 00 00'), 'secondary deleting-this adjustment B4')
    for address, target in ((0x425104, 0x421420), (0x42516a, 0x469820),
                            (0x4252b8, 0x46ac40), (0x4252bf, 0x424ef0),
                            (0x424908, 0x461d70), (0x42491c, 0x461eb0),
                            (0x424b88, 0x424840), (0x424bd6, 0x456310)):
        code = data(address, 5)
        check(code[0] == 0xe8 and address + 5 + struct.unpack('<i', code[1:])[0] == target,
              f'original CALL{address:08X}->{target:08X}')
    check(data(0x42494c, 3) == bytes.fromhex('FF 52 38'), 'renderer secondary slot14')
    check(data(0x424c1f, 3) == bytes.fromhex('FF 52 24'), 'renderable direct draw slot9')
    check(struct.unpack('<f', data(0x6dca98, 4))[0] == 0.0010000000474974513,
          'exact cull threshold')
    check(data(0x424d85, 5) == bytes.fromhex('8B 52 FC 89 10'), 'callback removal swaps last pointer into match')
    print(f'PASS {checks}/{checks}: PC-only render-node static anchors')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
