#!/usr/bin/env python3
"""Read-only fixed PC anchors for plane storage; never executes protected copy."""
from pathlib import Path
import struct
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local-data/research-cache/python'))
import capstone
from inspect_serializer_manager import read_pe, image_slice, sha256, PC_SHA256


def main():
    raw = (ROOT / 'local-data/pc-pristine/WinxClub.exe').read_bytes()
    base, sections = read_pe(raw)
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    count = 0
    def check(value, label):
        nonlocal count
        count += 1
        if not value:
            raise AssertionError(label)
    def data(address, size):
        return image_slice(raw, sections, address - base, size)
    def ins(address):
        value = next(decoder.disasm(data(address, 15), address))
        return value.mnemonic, value.op_str
    check(sha256(raw) == PC_SHA256, 'fixed PC image fingerprint')
    for address, target in ((0x46c88c, 0x45e870), (0x46cc77, 0x45e870),
                            (0x46cc90, 0x46adc0), (0x13b5e20, 0xa0d3e0),
                            (0x13b92c0, 0xa0d3e0), (0x46bd55, 0x45e530),
                            (0x46be0d, 0x45e530)):
        value = data(address, 5)
        check(value[0] == 0xe8 and address + 5 + struct.unpack('<i', value[1:])[0] == target,
              f'CALL{address:08X}->{target:08X}')
    for address, expected in (
        (0x46c894, ('mov', 'dword ptr [ebp + 0x10], ecx')),
        (0x46cc7f, ('mov', 'dword ptr [ebx + 0x10], edx')),
        (0x46b45b, ('mov', 'eax, ecx')),
        (0x46b45d, ('shr', 'eax, 1')),
        (0x46b46e, ('add', 'ecx, eax')),
        (0x46b421, ('mov', 'edx, 0xccccccc')),
        (0x46b4a2, ('push', '0x1a5')),
        (0x46b4aa, ('push', '0x6daf38')),
        (0x45dc6b, ('mov', 'dl, byte ptr [ecx + 0x10]')),
        (0x45dc6e, ('mov', 'byte ptr [eax + 0x10], dl')),
        (0x45dc71, ('add', 'ecx, 0x14')),
        (0x45dc74, ('add', 'eax, 0x14')),
        (0x412ea1, ('ret', '0x18')),
        (0x13df2f1, ('mov', 'bl, byte ptr [esp + 8]')),
        (0x13df33e, ('mov', 'dword ptr [ecx + 0x10], eax')),
        (0x13df343, ('mov', 'dword ptr [ecx + 0x10], 0')),
        (0x13df391, ('mov', 'byte ptr [ebp + esi + 0x10], bl')),
        (0x13df39f, ('ret', '4'))):
        check(ins(address) == expected, f'anchor{address:08X}: {expected}')
    check(ins(0x46bd60) == ('mov', 'dword ptr [esi + 0x10], edx'),
          'outer element constructor separately copies activeCount after45E530')
    check(data(0x6daf38, 80).split(b'\0')[0] ==
          b'z:\\sparkplug\\code\\sparkbase\\spSTL_allocator.h (vector allocator)',
          'original allocator diagnostic path, not original plane class identity')
    print(f'PASS {count}/{count}: PC plane storage and shared unresolved dispatcher anchors')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
