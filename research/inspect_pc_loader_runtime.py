#!/usr/bin/env python3
"""Fixed PC loader/FAT/RTTI instruction anchors, decoded from original bytes.

No decompiler boundaries, protected startup execution or PS2 extrapolation.
"""
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
import capstone
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256


def main():
    raw=(ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes();base,sections=read_pe(raw)
    decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);count=0
    def check(value,label):
        nonlocal count
        count+=1
        if not value:raise AssertionError(label)
    def data(address,size):return image_slice(raw,sections,address-base,size)
    def ins(address):
        value=next(decoder.disasm(data(address,15),address))
        return value.mnemonic,value.op_str
    check(sha256(raw)==PC_SHA256,'exact independently fixed PC fingerprint')
    for address,expected in (
        (0x13d6e7f,('push','0x58')),
        (0x41442e,('lea','ecx, [esi + 0x14]')),
        (0x414436,('mov','ecx, dword ptr [esi + 0x18]')),
        (0x414442,('mov','edx, dword ptr [eax + 0x10]')),
        (0x414445,('mov','eax, dword ptr [edx + 0x4c]')),
        (0x414451,('call','eax')),
        (0x4671e0,('mov','eax, dword ptr [esp + 4]')),
        (0x6d1c10,('push','0')),
        (0x6d1c12,('push','0x41a090')),
        (0x6d1c17,('push','0x75de50')),
        (0x6d1c21,('push','0x4fad24f1')),
        (0x6d1c26,('push','0x56ee563a')),
        (0x6d1c2b,('mov','ecx, 0x75d248')),
        (0x422d1d,('call','dword ptr [eax + 0x1c]')),
        (0x4229e8,('call','dword ptr [eax + 0x1c]')),
        (0x4229f5,('call','dword ptr [edx + 8]')),
        (0x465f20,('mov','eax, dword ptr [ecx + 0x54]')),
        (0x466b4d,('lea','ecx, [esi + 0x20]')),
        (0x466b5d,('add','esi, 0x48'))):
        check(ins(address)==expected,f'{address:08X} instruction {expected}')
    for address,target in ((0x414431,0x5e79e0),(0x6d1c30,0x412ff0),
                           (0x422d0d,0x4aa430),(0x422d2b,0x422940),
                           (0x422c20,0x466b90),(0x422c4e,0x465cd0),
                           (0x422d71,0x466760),(0x422d79,0x466870)):
        b=data(address,5)
        check(b[0]==0xe8 and address+5+struct.unpack('<i',b[1:])[0]==target,
              f'CALL {address:08X}->{target:08X}')
    for address,target in ((0x6e0b00,0x43dfe0),(0x6e0b04,0x5a7db0),(0x6e0b08,0x43ecc0),
                           (0x6e0b28,0x467550),(0x6e7f38,0x465c70),(0x6e7f3c,0x465ca0)):
        check(struct.unpack('<I',data(address,4))[0]==target,f'vtable {address:08X}->{target:08X}')
    print(f'PASS {count}/{count}: PC generic loader/FAT/RTTI fixed instruction anchors')
    return 0


if __name__=='__main__':raise SystemExit(main())
