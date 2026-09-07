#!/usr/bin/env python3
"""Read-only PC exact-ID scene, SkyBox camera ownership and manager anchors."""
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
from inspect_executable_architecture import scan_pe
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256

def main():
    path=ROOT/'local-data/pc-pristine/WinxClub.exe';raw=path.read_bytes()
    base,sections=read_pe(raw);checks=0
    def check(value,label):
        nonlocal checks
        checks+=1
        if not value:raise AssertionError(label)
    def data(address,size):return image_slice(raw,sections,address-base,size)
    check(sha256(raw)==PC_SHA256,'original PC hash')
    records={r['class_name']:r for r in scan_pe(path,0x12ff0)['registered_types']}
    for name,identity,parent,factory in (
            ('spSkyBox',0x7a7124af,0x603625d0,0x49e4c0),
            ('spSkyBoxManager',0x61c23595,0x415352a1,0x48dc80),
            ('spProjectionManager',0x24a010da,0x20a72504,0),
            ('spPCProjectionManager',0xe10d6fc2,0x24a010da,0x4c5ce0),
            ('spLensFlareManager',0x782e7d46,0x20a72504,0),
            ('spPCLensFlareManager',0x4838786b,0x782e7d46,0x4c7240)):
        record=records[name]
        check((record['class_hash'],record['base_class_hash'],record['constructor_arg5_va'])==
              (identity,parent,factory),'actual registered hierarchy/factory '+name)
    for address,values in (
        (0x6ec460,(0x48dae0,0x5b7a00,0x48dce0,0x40ece0,0x48dad0,0x408350,0x408370)),
        (0x6eecd4,(0x4d74a0,0x4248d0,0x424c30,0x49e4b0,0x424790,0x4247b0)),
        (0x6eecec,(0x49e590,0x420b40,0x49e540,0x49e5b0,0x49e400,0x408350,0x408370,
                    0x49e430,0x4249f0,0x424af0,0x420610,0x421330,0x49e440,0x424e70)),
        (0x6f2a00,(0x4c5da0,0x5b7a00,0x4c5d50,0x413120,0x4c5cc0,0x408350,0x408370,
                    0x45a290,0x4cde70,0x4cdf60,0x45a3d0,0x5a7db0,0x5a7db0,0x4cdea0,0x4c5ca0,0x4c5cb0)),
        (0x6f2a5c,(0x4c71f0,0x5b7a00,0x4c72a0,0x413120,0x4c6d80,0x408350,0x408370,
                    0x4c5e30,0x4c67b0,0x4c7210,0x4c6910,0x4c5dc0,0x4c6880))):
        check(struct.unpack(f'<{len(values)}I',data(address,4*len(values)))==values,
              f'actual table{address:08X}')
    for address,target in ((0x49e448,0x4250f0),(0x48dbc9,0x421a60),
                            (0x5da702,0x48db40),(0x5da95d,0x48da30),
                            (0x5f9f48,0x48db40),(0x5f886c,0x48da30),
                            (0x4cde83,0x4243d0),(0x4c68f6,0x4cea10)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'actual CALL{address:08X}->{target:08X}')
    check(data(0x5da6dd,12)==bytes.fromhex('8B481C8B40188B4030894814'),
          'game sets engine DefaultCamera as SkyBoxManager borrowed root')
    check(data(0x5f9f23,12)==bytes.fromhex('8B50188B481C8B4230894814'),
          'second game block confirms same default camera ownership')
    check(data(0x49e44d,9)==bytes.fromhex('8B4E40898E8C000000'),
          'SkyBox overrides inherited world orientation with local orientation')
    check(data(0x4c5e51,5)==bytes.fromhex('3D6A087688'),
          'flare capability compares exact8876086A, not sign of HRESULT')
    check(data(0x4d74a0,5)==bytes.fromhex('B001C20800'), 'regular sky support draw is true/no-op')
    print(f'PASS {checks}/{checks}: PC specialized scene static anchors')
    return 0
if __name__=='__main__':raise SystemExit(main())
