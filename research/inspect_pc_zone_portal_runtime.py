#!/usr/bin/env python3
"""Read-only pristine PC portal identity, exact tables and processing edges."""
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
    rows={r['class_name']:r for r in scan_pe(path,0x12ff0)['registered_types']}
    for name,identity,parent,factory in (('spZonePortal',0x6523ac37,0x44de07fd,0x481370),
                                        ('spZonePortalNode',0xabb5ab2c,0x695c0f65,0x481810)):
        row=rows[name]
        check((row['class_hash'],row['base_class_hash'],row['constructor_arg5_va'])==
              (identity,parent,factory),'original class/base/factory '+name)
    check(struct.unpack('<8I',data(0x6ebb10,32))==
          (0x481110,0x5b7a00,0x4813f0,0x413120,0x481100,0x408350,0x408370,0x481270),
          'portal exact8 slots, Named-only copy plus DebugDraw')
    check(struct.unpack('<14I',data(0x6ebb44,56))==
          (0x4815a0,0x420b40,0x481870,0x421f80,0x481540,0x408350,0x408370,
           0x420e30,0x420e60,0x4212f0,0x420610,0x421330,0x421420,0x421640),
          'portal Node exact14, inherited world/Enabled and Node-only copy')
    for address,slot in ((0x481130,0x13b1d00),(0x4810a0,0x13b31a4),(0x481550,0x13b2cd4),
                          (0x481930,0x13b2e98),(0x481370,0x13b245c),(0x481810,0x13b1918)):
        check(data(address,6)==b'\xff\x25'+struct.pack('<I',slot),'exact protected entry '+hex(address))
    for address,target in ((0x44df0a,0x481130),(0x44e6fe,0x481930),
                            (0x46ce2a,0x491aa0),(0x46cffa,0x471420),
                            (0x471479,0x426a40),(0x426a7b,0x41d2d0),
                            (0x46cc77,0x45e870),(0x46cd56,0x46c4d0),(0x46d0d5,0x46c4d0),
                            (0x491c43,0x60db63)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'CALL{address:08X}->{target:08X}')
    check(data(0x46ca1d,3)==bytes.fromhex('8A4720'),'visibility tests Open20')
    check(data(0x46ca41,3)==bytes.fromhex('894734'),'portal marked before side/polygon test')
    check(data(0x44df2f,3)==bytes.fromhex('894514'),'reader directly stores borrowed destination14')
    check(b'spZonePortal.cpp' not in raw and b'spZonePortal.h' not in raw and
          b'spZonePortalNode.cpp' not in raw and b'spZonePortalNode.h' not in raw,
          'limited ASCII implementation/header names absent; do not invent exact source paths')
    check(b'spZonePortalSerializer.cpp' not in raw and b'spZonePortalNodeSerializer.cpp' in raw,
          'only Node serializer TU path survives this limited ASCII search')
    for name in (b'GetDestinationZone()',b'IsOpen()',b'GetPolygonVertex( i )',b'GetPolygonVertexCount()',
                  b'pZonePortalNode->GetZonePortal( i )'):
        check(name in raw,'original getter name in diagnostic: '+name.decode())
    check(data(0x60dfac,2)==b'\xff\x25','imported dllonexit boundary, not unknown geometry helper')
    print(f'PASS {count}/{count}: PC ZonePortal anchors');return 0


if __name__=='__main__':raise SystemExit(main())
