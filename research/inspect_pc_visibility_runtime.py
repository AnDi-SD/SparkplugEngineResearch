#!/usr/bin/env python3
"""Read-only PC Visibility identity, clone, plane and caller anchors."""
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
    check(sha256(raw)==PC_SHA256,'pristine PC fingerprint')
    row=next(r for r in scan_pe(path,0x12ff0)['registered_types']
             if r['class_name']=='spVisibilityManager')
    check((row['class_hash'],row['base_class_hash'],row['constructor_arg5_va'])==
          (0x3d7f4387,0x415352a1,0x46d1c0),'Visibility original identity/base/factory')
    table=(0x46c4b0,0x5b7a00,0x46d220,0x40ece0,0x46c330,0x408350,0x408370)
    check(struct.unpack('<7I',data(0x6e8cdc,28))==table,'bounded primary seven slots')
    check(struct.unpack('<I',data(0x6e8cd8,4))[0]==0x46c340,'separate support slot')
    for address,target in ((0x46d224,0x46d1c0),(0x46d246,0x412f70),
                            (0x46d2a7,0x46b270),(0x46d2b1,0x46b300),
                            (0x46d387,0x46b130),(0x46d3df,0x46bb80),
                            (0x46d401,0x46bb80),(0x46d412,0x46bb80),
                            (0x46d423,0x46bb80),(0x46d434,0x46bb80),
                            (0x46d445,0x46bb80),(0x46d495,0x46b870),
                            (0x46d4cb,0x46c4d0),(0x46c524,0x4902d0),
                            (0x46c58a,0x4902d0),(0x46c62d,0x4902d0)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'CALL{address:08X}->{target:08X}')
    check(data(0x46d250,3)==bytes.fromhex('FF500C'),'clone inherited-copy dispatch')
    check(data(0x46c500,3)==bytes.fromhex('89457C'),'visited root stamp7C')
    check(data(0x4902d0,7)==bytes.fromhex('8B411085C07505'),'outside helper cached count gate')
    check(data(0x46d3fa,6)==bytes.fromhex('8D87D4010000'),'second plane camera far1D4')
    check(data(0x46d45f,3)==bytes.fromhex('8A4821'),'Debug21 selects unclipped traversal')
    print(f'PASS {count}/{count}: original PC Visibility anchors')
    return 0


if __name__=='__main__':raise SystemExit(main())
