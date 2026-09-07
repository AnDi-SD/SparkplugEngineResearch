#!/usr/bin/env python3
"""Read-only original Octree identity/query/table/serializer anchors."""
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
from inspect_executable_architecture import scan_pe
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256


def main():
    path=ROOT/'local-data/pc-pristine/WinxClub.exe'
    raw=path.read_bytes();base,sections=read_pe(raw);count=0
    def check(value,label):
        nonlocal count
        count+=1
        if not value:raise AssertionError(label)
    def data(address,size):return image_slice(raw,sections,address-base,size)
    check(sha256(raw)==PC_SHA256,'pristine fingerprint')
    row=next(r for r in scan_pe(path,0x12ff0)['registered_types'] if r['class_name']=='spOctreeNode')
    check((row['class_hash'],row['base_class_hash'],row['constructor_arg5_va'])==
          (0x21a70829,0x67672341,0x41a760),'original class/base/factory')
    table=(0x449ae0,0x5b7a00,0x41b100,0x40ece0,0x449410,0x408350,0x408370,
           0x449b00,0x425a80,0x425ae0,0x449c10,0x425b60,0x425bc0,0x449d30,
           0x425c40,0x425ca0,0x449e50,0x449430,0x449530,0x449690,0x44aa30,
           0x44ac60,0x449a90,0x449520,0x4258a0,0x449ec0,0x44a2f0,0x44a690,
           0x44a730,0x449a60,0x44a880,0x44a9b0,0x4269c0)
    check(struct.unpack('<33I',data(0x6e4420,132))==table,'bounded33 complete primary entries')
    check(data(0x41a760,6)==bytes.fromhex('FF25CC193B01'),'factory protected slot13B19CC')
    check(data(0x4493b0,6)==bytes.fromhex('FF250C2F3B01'),'ctor protected slot13B2F0C')
    check(data(0x459036,5)==bytes.fromhex('68C8000000'),'decoded factory exactC8 allocation')
    check(data(0x740138,24)==bytes.fromhex('0f0f0f0ff0f0f0f03333cccc3333cccc55aa55aa55aa55aa'),
          'exact sphere axis-line masks, not generic octant overlap')
    check(struct.unpack('<8I',data(0x740150,32))==
          (0xfab888,0xde2ac1,0xbe1cc2,0x9a8e8b,0x6571ac,0x4c633d,0x21d53e,0x08c777),
          'original eight packed traversal orders')
    for address,target in ((0x449ae3,0x449420),(0x41b104,0x41a760),(0x41b126,0x412f70),
                            (0x449ce3,0x426690),(0x449df9,0x4266b0),
                            (0x449bc4,0x426670),(0x44977c,0x4494e0),
                            (0x44982c,0x4494e0),(0x4498e3,0x4494e0),
                            (0x46c88c,0x45e870),(0x46bac9,0x46b870)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'CALL{address:08X}->{target:08X}')
    check(data(0x41b130,3)==bytes.fromhex('FF500C'),'clone inherited Base copy slot')
    check(data(0x449c36,5)==bytes.fromhex('A900003000'),'static RenderNode special billboard mask300000')
    check(data(0x4496ed,6)==bytes.fromhex('898D90000000'),'ray scratch starts90, not serializable geometry')
    check(data(0x44c9ee,6)==bytes.fromhex('898784000000'),'serializer writes pivot84 directly')
    check(b'spOctreeNode.cpp' not in raw and b'spOctreeNode.h' not in raw,
          'limited ASCII source-name search in this pristine PC build is negative')
    print(f'PASS {count}/{count}: PC Octree anchors');return 0


if __name__=='__main__':raise SystemExit(main())
