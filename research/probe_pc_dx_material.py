#!/usr/bin/env python3
"""Bounded actual PC DX material header, defaults, scalar copy and destruction."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

def main(mode):
    if mode not in {'header','scalar-clone'}:raise ValueError('small DX material case')
    checks=0
    def check(ok,text):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(text)
    f=PCFileBytesFixture(struct.pack('<II',0x12345678,0xabcdef01));p=f.p
    obj=f.call(0x42f4c0,args=(f.stream,)) if mode=='header' else f.call(0x4a9460)
    check(f.allocations[obj]==0xbc and p.uint(obj)==0x6ef264 and p.uint(obj+0x14)==0x6ef238,'actual DXMaterial BC, primary and adjusted-this interface')
    check(f.call(0x4a92e0,this=obj)==0x7630e8,'actual DXMaterial runtime record getter')
    check(bytes(p.mu.mem_read(obj+0x78,64))==struct.pack('<16f',1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0),'DX colors use transparent black, unlike MaterialData')
    check(p.uint(obj+0xb8)==0xcccccccc,'native ctor leaves specular power untouched; CC is allocator poison NOT default')
    if mode=='header':check(f.position==8 and not f.errors,'header consumes both arbitrary words but ignores identity and marker')
    else:
        f.call(0x52fd90,this=0x755588)
        values=struct.pack('<16fI',*[v/16 for v in range(16)],0x40600000)
        p.mu.mem_write(obj+0x78,values);p.put_uint(obj+0x18,19);p.mu.mem_write(obj+0x6c,b'\x02\x03')
        p.put_uint(obj+4,0x12345678);p.put_uint(obj+0x70,0x99887766)
        clone=f.call(0x4a94c0,this=obj)
        check(clone!=obj and f.allocations[clone]==0xbc,'actual distinct DX clone')
        check(bytes(p.mu.mem_read(clone+0x78,68))==values,'DX clone copies all seventeen payload words unlike MaterialData blank clone')
        check(p.uint(clone+0x18)==19 and bytes(p.mu.mem_read(clone+0x6c,2))==b'\x02\x03','DX base-copy propagates states and raw flags')
        check(p.uint(clone+4)==0 and p.uint(clone+0x70)==0,'base auxiliary pointer/opaque runtime70 not copied by Material base-copy')
        p.put_uint(obj+4,0) # external auxiliary-pointer sentinel is not an owned native object
        f.call(p.uint(p.uint(clone)),this=clone,args=(1,));f.call(0x6d7db0)
    f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    for address in (0x74e060,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native allocations released')
    print(f'PASS {checks}/{checks}: actual DX material {mode}; no renderer/GPU')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
