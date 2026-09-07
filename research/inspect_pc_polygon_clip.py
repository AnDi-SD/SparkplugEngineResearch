#!/usr/bin/env python3
"""Read-only PC polygon clip/count/copy/pool and fixed-stack anchors."""
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256
import capstone


def main():
    raw=(ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()
    base,sections=read_pe(raw);count=0
    def check(value,label):
        nonlocal count
        count+=1
        if not value:raise AssertionError(label)
    def data(address,size):return image_slice(raw,sections,address-base,size)
    check(sha256(raw)==PC_SHA256,'pristine fingerprint')
    for address,target in ((0x491b9d,0x491a30),(0x491bb3,0x491660),(0x491c81,0x491660),
                            (0x491d84,0x491660),(0x491c1d,0x493ac0),(0x491c43,0x60db63),
                            (0x4916c8,0x491570)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'CALL{address:08X}->{target:08X}')
    check(data(0x491a30,6)==bytes.fromhex('FF2578323B01'),'copy protected slot13B3278')
    decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    def instruction(address):
        i=next(decoder.disasm(data(address,15),address));return i.mnemonic,i.op_str
    check(instruction(0x491b11)==('fst','qword ptr [esp + ecx*8 + 0x22c]'),'distance array double128')
    check(instruction(0x491b21)==('mov','dword ptr [esp + ecx*4 + 0x2c], 1'),'side array UInt32 at2C')
    check(instruction(0x491b77)==('fstp','qword ptr [esp + ecx*8 + 0x22c]'),'closing distance writes element[count]')
    check(instruction(0x491b7e)==('mov','dword ptr [esp + ecx*4 + 0x2c], edx'),'closing side writes element[count]')
    check(instruction(0x491e2a)==('mov','ecx, dword ptr [esp + 0x62c]'),'SEH record immediately after128 doubles')
    check((0x22c-0x2c)//4==(0x62c-0x22c)//8==128,'safe input<=127 derived, no unsafe128 run')
    loop=list(decoder.disasm(data(0x491dc0,0x491e26-0x491dc0),0x491dc0))
    check(all(not(i.mnemonic=='mov' and i.op_str.startswith('ebp,')) for i in loop),
          'postprojection loop never advances point EBP')
    check(instruction(0x491d64)==('mov','ebp, dword ptr [ebp + 0x18]'),'output append loop does advance/wrap before postpass')
    check(instruction(0x491d8c)==('mov','ecx, 0xb') and instruction(0x491d93)==('rep movsd','dword ptr es:[edi], dword ptr [esi]'),
          'in-place branch copies complete11-word scratch record')
    check(instruction(0x491d9c)==('mov','dword ptr [ecx + 4], eax'),'restores destination identity only after whole-record move')
    check(instruction(0x49166c)==('mov','dword ptr [esi + 0xc], eax'),'count0 on entry clears old stale head')
    check(instruction(0x49170e)==('mov','dword ptr [esi + 8], ebx'),'resize commits requested logical count')
    check(b'spPolygon.cpp' not in raw and b'spPolygon.h' not in raw and b'spConvexPolygon.cpp' not in raw,
          'limited likely ASCII class-source names absent, helper kept unnamed')
    print(f'PASS {count}/{count}: PC polygon clip anchors');return 0


if __name__=='__main__':raise SystemExit(main())
