#!/usr/bin/env python3
"""PC static placement Zone asymmetry and borrowed Occlusion path anchors."""
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256
import capstone


def main():
    raw=(ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes();base,sections=read_pe(raw)
    decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);count=0
    def check(value,label):
        nonlocal count
        count+=1
        if not value:raise AssertionError(label)
    def data(a,n):return image_slice(raw,sections,a-base,n)
    def instruction(a):
        i=next(decoder.disasm(data(a,15),a));return i.mnemonic,i.op_str
    check(sha256(raw)==PC_SHA256,'pristine fingerprint')
    for a,expected in ((0x480ad3,('mov','eax, dword ptr [esi + 0x60]')),
                       (0x480ae9,('call','dword ptr [eax + 0x48]')),
                       (0x449aa2,('call','dword ptr [eax + 0x48]')),
                       (0x480638,('mov','eax, dword ptr [esi + 0x60]')),
                       (0x480643,('mov','eax, dword ptr [ebx + 0xb0]')),
                       (0x480649,('shr','eax, 0xa')),
                       (0x449d3b,('mov','eax, dword ptr [esi + 0x60]')),
                       (0x449d46,('mov','eax, dword ptr [ebx + 0xb0]')),
                       (0x449d4c,('shr','eax, 0xa')),
                       (0x426765,('inc','word ptr [esi + 8]')),
                       (0x4267e5,('mov','dword ptr [esi + 0x88], edx'))):
        check(instruction(a)==expected,f'anchor{a:08X}: {expected}')
    for a,target in ((0x480b53,0x426740),(0x480692,0x4266b0)):
        code=data(a,5)
        check(code[0]==0xe8 and a+5+struct.unpack('<i',code[1:])[0]==target,f'CALL{a:08X}->{target:08X}')
    code=list(decoder.disasm(data(0x449a90,0x46),0x449a90))
    check(code[-1].mnemonic=='ret' and not any('0x60]' in i.op_str for i in code),
          'complete Octree static insertion has no Zone60 access, unlike BSP')
    print(f'PASS {count}/{count}: PC spatial consumer anchors');return 0


if __name__=='__main__':raise SystemExit(main())
