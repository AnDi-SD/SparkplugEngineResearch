#!/usr/bin/env python3
"""Read-only SceneRender/Shadow identity and actual gate/state-cache anchors."""
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
from inspect_executable_architecture import scan_pe
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256


def main():
    path=ROOT/'local-data/pc-pristine/WinxClub.exe'
    raw=path.read_bytes()
    base,sections=read_pe(raw)
    count=0
    def check(value,label):
        nonlocal count
        count+=1
        if not value:raise AssertionError(label)
    def data(address,size):return image_slice(raw,sections,address-base,size)
    check(sha256(raw)==PC_SHA256,'pristine fingerprint')
    records={r['class_name']:r for r in scan_pe(path,0x12ff0)['registered_types']}
    for name,identity,parent,factory in (
            ('spShadowVolumeManager',0x63fea321,0x20a72504,0),
            ('spDXShadowVolumeManager',0x04680bc1,0x63fea321,0x4a9030),
            ('spDXRenderer',0x46004ee1,0x2d9c0296,0)):
        row=records[name]
        check((row['class_hash'],row['base_class_hash'],row['constructor_arg5_va'])==
              (identity,parent,factory),'actual identity '+name)
    check(struct.unpack('<9I',data(0x6ef138,36))==
          (0x4a9010,0x5b7a00,0x4a90b0,0x413120,0x4a7c60,0x408350,0x408370,
           0x4a8570,0x4a85f0),'complete bounded DXShadow primary9')
    check(struct.unpack('<I',data(0x6ef134,4))[0]==0x4a7c70,'separate shadow support deleting adapter')
    for address,target in ((0x4a9013,0x4a7c30),(0x4a90b4,0x4a9030),
                            (0x4a90d6,0x412f70),(0x45eccb,0x46d270),
                            (0x45f0a6,0x456910),(0x45f119,0x4a9030),
                            (0x45f140,0x454850),(0x45f2c1,0x40f9a0),
                            (0x4a8634,0x4b0a90),(0x4a8eea,0x4b0a90),
                            (0x4a8efc,0x4b0a90)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'CALL{address:08X}->{target:08X}')
    check(data(0x45f194,6)==bytes.fromhex('0F842C010000'),
          'Debug18 false jumps past terminal event to successful return')
    check(data(0x45ec86,5)==bytes.fromhex('FF503C84C0'),'Scene camera apply return gate')
    check(data(0x45f12f,5)==bytes.fromhex('FF522084C0'),'Scene real Shadow return gate')
    check(data(0x4a8615,6)==bytes.fromhex('8A8631020000'),'Shadow reads camera branch231')
    check(data(0x4b0ab1,6)==bytes.fromhex('FF91E4000000'),'native D3D state slotE4')
    check(data(0x4b0ab7,7)==bytes.fromhex('899CBEF4E40000'),
          'cache write follows device call without HRESULT check')
    print(f'PASS {count}/{count}: original Scene/Shadow/DX state anchors')
    return 0


if __name__=='__main__':raise SystemExit(main())
