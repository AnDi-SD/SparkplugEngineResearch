"""Compare the precise PC ABI regions used by the shader-key observer."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[2]


def region(data,address,size):
    pe=struct.unpack_from('<I',data,60)[0]
    if data[pe:pe+4]!=b'PE\0\0':raise ValueError('PE signature')
    optional=pe+24
    if struct.unpack_from('<H',data,optional)[0]!=0x10b:raise ValueError('PE32 required')
    base=struct.unpack_from('<I',data,optional+28)[0]
    sections=optional+struct.unpack_from('<H',data,pe+20)[0]
    for i in range(struct.unpack_from('<H',data,pe+6)[0]):
        _,va,length,offset=struct.unpack_from('<IIII',data,sections+40*i+8)
        delta=address-base-va
        if 0<=delta and delta+size<=length:
            result=data[offset+delta:offset+delta+size]
            if len(result)==size:return result
    raise ValueError('Region must lie within one file-backed section')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if not args.output.resolve().is_relative_to(ROOT/'local-data'):raise ValueError('Local evidence only')
    paths=[ROOT/'local-data/pc-pristine/WinxClub.exe',ROOT/'local-data/Winx Club/WinxClubDebug.exe']
    data=[p.read_bytes() for p in paths]
    hashes=[hashlib.sha256(d).hexdigest() for d in data]
    if hashes!=['3f022480bf55045da4bf692e4bc8862ed38fc024e8a964a558fbdfdf646dfc4f',
                'c27ea9db4228781a12a90ae808807d4af1397a7e40dd8f5fff28f3c87cc62cdb']:
        raise ValueError('Unqualified EXE')
    rows=[]
    for name,address,size in [('manager_select',0x4c8980,0x590),('renderer_key_region',0x4be310,0x198),
                              ('device_create_region',0x4ca030,0x50),('manager_vtable',0x6f2de4,40)]:
        parts=[region(d,address,size) for d in data]
        rows.append(dict(name=name,address=address,bytes=size,equal=parts[0]==parts[1],
                         sha256=hashlib.sha256(parts[1]).hexdigest()))
    result=dict(schema=1,images=[dict(path=str(p),sha256=h) for p,h in zip(paths,hashes)],regions=rows,
                scope='File-backed ABI byte identity; original CP38/57 establishes thiscall(mask, lightTypes) and returned object+0x50. No machine-code relocation.')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
    print(json.dumps(rows))
    return int(not all(r['equal'] for r in rows))


if __name__=='__main__':raise SystemExit(main())
