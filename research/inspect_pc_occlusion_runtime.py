#!/usr/bin/env python3
"""Read-only original Occlusion identity, table, lifecycle and consumer anchors."""
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
    row=next(r for r in scan_pe(path,0x12ff0)['registered_types'] if r['class_name']=='spOcclusionVolume')
    check((row['class_hash'],row['base_class_hash'],row['constructor_arg5_va'])==
          (0x43d24430,0x695c0f65,0x470a70),'original identity and direct base/factory')
    check(struct.unpack('<14I',data(0x6e8de0,56))==
          (0x46ffc0,0x420b40,0x470ad0,0x421f80,0x46fd70,0x408350,0x408370,
           0x46d7e0,0x46d520,0x4212f0,0x420610,0x421330,0x46dd90,0x421640),
          'bounded14 slots, Node-only copy and inherited Enabled')
    check(data(0x470a86,5)==bytes.fromhex('68B8010000'),'actual allocation1B8')
    for address,target in ((0x470aab,0x46fe60),(0x46ffc3,0x46fc50),
                            (0x470ad4,0x470a70),(0x470af6,0x412f70),
                            (0x46dda4,0x421420),(0x46ddf7,0x420660),
                            (0x46df32,0x46dcf0),(0x47113f,0x460d90),
                            (0x471158,0x470e30),(0x44f538,0x470fe0),
                            (0x470303,0x46ffe0),(0x470463,0x471420),
                            (0x45ee8c,0x4702e0),(0x45ef9d,0x4903e0)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'CALL{address:08X}->{target:08X}')
    check(data(0x470b00,3)==bytes.fromhex('FF500C'),'clone dispatches inherited Node copy slot')
    check(data(0x45ee91,3)==bytes.fromhex('8B4748'),'Scene does not gate on plane-builder AL result')
    check(data(0x46f907,6)==bytes.fromhex('889EB4010000'),'geometry clear resets initialized byte')
    check(data(0x460c49,6)==bytes.fromhex('89359CFF7500'),'UInt16 weld sets shared VB comparator context')
    check(data(0x6e8d14,160).split(b'\0')[0]==b'Z:\\Sparkplug\\Code\\Sparkplug\\spOcclusionVolume.cpp',
          'exact original class translation-unit path, not just serializer path')
    check(struct.unpack('<f',data(0x6e8d10,4))[0]==struct.unpack('<f',struct.pack('<f',.001))[0],
          'actual geometry componentwise epsilon0.001')
    print(f'PASS {count}/{count}: PC Occlusion anchors')
    return 0


if __name__=='__main__':raise SystemExit(main())
