#!/usr/bin/env python3
"""Read-only original PC BSP registration/table/reader/query anchors."""
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
from inspect_executable_architecture import scan_pe
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256


def main():
    path=ROOT/'local-data/pc-pristine/WinxClub.exe';raw=path.read_bytes()
    base,sections=read_pe(raw);count=0
    def check(value,label):
        nonlocal count
        count+=1
        if not value:raise AssertionError(label)
    def data(address,size):return image_slice(raw,sections,address-base,size)
    check(sha256(raw)==PC_SHA256,'pristine fingerprint')
    row=next(r for r in scan_pe(path,0x12ff0)['registered_types'] if r['class_name']=='spBSPNode')
    check((row['class_hash'],row['base_class_hash'],row['constructor_arg5_va'])==
          (0x7362ab22,0x67672341,0x480b90),'original BSP/Partition/factory')
    check(row['registration_object_va']==0x7613f8 and row['base_registration_va']==0x75e1b8,
          'exact original type records')
    check(struct.unpack('<33I',data(0x6eba30,132))==
          (0x480450,0x5b7a00,0x480c10,0x40ece0,0x4801f0,0x408350,0x408370,
           0x480470,0x425a80,0x425ae0,0x480540,0x425b60,0x425bc0,0x480630,0x425c40,0x425ca0,
           0x480710,0x480780,0x4807d0,0x480360,0x480c60,0x4d6550,0x480ad0,0x480200,0x4258a0,
           0x480830,0x4d6550,0x4d6550,0x425770,0x4d6550,0x425770,0x425770,0x4269c0),
          'exact33 primary slots')
    check(data(0x480b90,6)==b'\xff\x25'+struct.pack('<I',0x13b23e4),'protected factory entry')
    check(struct.unpack('<2I',data(0x740374,8))==(8,1),'only two packed child orders')
    for address,value in ((0x6eba2c,.001),(0x6e44a4,1e-5),(0x6ebab4,-1e-5)):
        check(data(address,4)==struct.pack('<f',value),'exact epsilon '+hex(address))
    for address,target in ((0x44cfd5,0x480210),(0x44d038,0x44cda0)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'CALL{address:08X}->{target:08X}')
    for name in (b'pBSPNode->GetPolygonVertex( i )',b'pBSPNode->GetPolygonVertexCount()',
                  b'pBSPNode->GetPlane().m_fConstant',b'pBSPNode->GetPlane().m_vNormal'):
        check(name in raw,'original diagnostic getter '+name.decode())
    check(b'spBSPNode.cpp' not in raw and b'spBSPNode.h' not in raw,
          'limited ASCII source/header search absent; inferred reconstructed paths')
    check(b'spBSPNodeSerializer.cpp' not in raw and b'spBSPNodeSerializer' in raw,
          'serializer class name survives, but limited ASCII TU path does not')
    print(f'PASS {count}/{count}: PC BSP anchors');return 0


if __name__=='__main__':raise SystemExit(main())
