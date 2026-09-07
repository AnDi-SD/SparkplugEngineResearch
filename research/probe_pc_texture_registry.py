#!/usr/bin/env python3
"""Six real PC startup registrations for ONE wire TextureData ID; no cold RTTI."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture,empty_manager

def main():
    f=PCFileBytesFixture(b'');p=f.p;manager,_=empty_manager(f);p.put_uint(0x75dde8,manager);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    for entry in (0x6d1850,0x6d1880,0x6d18b0,0x6d18e0,0x6d1910,0x6d1940):
        f.call(entry);print('TEXTURE_STARTUP',hex(entry),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    selected=[]
    for platform,expected in ((1,0x6ddd90),(2,0x6dd648),(4,0x6dd648),(8,0x6ddba8)):
        for operation in (1,2):
            p.put_uint(manager+0x10,platform);p.put_uint(manager+0x14,operation)
            obj=f.call(0x4224f0,this=manager,args=(0x78ea082b,))
            check(bool(obj) and p.uint(obj)==expected,'original registration dispatches same wire ID according to platform/operation')
            own_tag=f.call(p.uint(p.uint(obj)+0x28),this=obj)
            selected.append([platform,operation,hex(p.uint(obj)),hex(own_tag)])
            if platform in (2,4):check(own_tag==0x0b1c67bb,'serializer virtual identifier is NOT its sole registry wire key')
            check(f.call(0x4224f0,this=manager,args=(0x0b1c67bb,))==0,'no invented DXTextureData wire-key registration')
    print('TEXTURE_REGISTRY',selected,flush=True)
    f.call(0x4228a0,this=manager)
    for address in (0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'manager destroys every actual startup serializer and registry node')
    print(f'PASS {checks}/{checks}: actual PC texture registry; PS2-masked entries are PC executable evidence, not PS2 runtime')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
