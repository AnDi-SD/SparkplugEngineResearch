#!/usr/bin/env python3
"""Actual PC material interface-adjustment, blank clone and intrusive pass edits."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

def main(mode):
    if mode not in {'interface-clone','pass-owners'}:raise ValueError('bounded material runtime mode')
    checks=0
    def check(ok,text):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(text)
    f=PCFileBytesFixture(b'');p=f.p
    if mode=='interface-clone':f.call(0x52fd90,this=0x755588)
    obj=f.call(0x41a390)
    check(f.allocations[obj]==0xbc and p.uint(obj+0x10)==0,'PC complete BC and physical NULL name')
    defaults=bytes(p.mu.mem_read(obj+0x78,68))
    check(struct.unpack('<11I',bytes(p.mu.mem_read(obj+0x18,44)))==(0,0,1,2,1,1,3,0,4,1,6),'PC states start18, not PS2 aligned20')
    check(p.uint(obj+0x44)==0xcccccccc,'opaque44 really left untouched by PC ctor')
    if mode=='interface-clone':
        interface=obj+0x14;vt=p.uint(interface);buffer=p.allocate(16)
        for i,(offset,getter,setter) in enumerate(((0x78,3,4),(0x88,1,2),(0x98,7,8),(0xa8,5,6))):
            value=struct.pack('<4I',0x3e800000+i,0x80000000,0x7fc12345,0x3f000000)
            p.mu.mem_write(buffer,value);f.call(p.uint(vt+setter*4),this=interface,args=(buffer,))
            check(bytes(p.mu.mem_read(obj+offset,16))==value,'actual secondary setter uses complete+14 adjusted this')
            p.mu.mem_write(buffer,bytes(16));result=f.call(p.uint(vt+getter*4),this=interface,args=(buffer,))
            check(result==buffer and bytes(p.mu.mem_read(buffer,16))==value,'actual getter returns output address and unchanged raw bits')
        name=p.allocate(32);p.mu.mem_write(name+8,b'\x01material-prefix\0');p.put_uint(obj+0x10,name)
        before=bytes(p.mu.mem_read(obj+0x78,68));clone=f.call(0x41acf0,this=obj)
        check(clone!=obj and f.allocations[clone]==0xbc,'actual distinct material clone BC')
        check(p.uint(clone+0x10)==0 and bytes(p.mu.mem_read(clone+0x78,68))==defaults,'leaf no-op copy loses name and changed colors')
        check(bytes(p.mu.mem_read(obj+0x78,68))==before and bytes(p.mu.mem_read(name+8,1))==b'\x01','source and external name unchanged by blank clone')
        p.put_uint(obj+0x10,0);f.call(p.uint(p.uint(clone)),this=clone,args=(1,));f.call(0x6d7db0)
    else:
        a=f.call(0x45f610);b=f.call(0x45f610)
        check(f.call(0x423960,this=obj,args=(0,a))&255==1 and p.uint(a+8)&65535==1,'native material retains first pass')
        check(f.call(0x423960,this=obj,args=(2,b))&255==1 and p.uint(obj+0x48)==3 and p.uint(b+8)&65535==1,'native permits sparse slot2 with count3')
        check(f.call(0x423960,this=obj,args=(0,0))&255==1 and a in f.freed,'remove sole-owned slot releases/deletes pass')
        check(p.uint(obj+0x48)==2 and p.uint(obj+0x4c)==0 and p.uint(obj+0x50)==b and p.uint(b+8)&65535==1,'all slots shift; sparse NULL retained, exactly one B owner')
        check(f.call(0x423960,this=obj,args=(6,0))&255==1 and p.uint(obj+0x48)==2,'NULL beyond active count leaves count')
        check(f.call(0x423960,this=obj,args=(1,0))&255==1 and b in f.freed and p.uint(obj+0x48)==1,'remove final live pass decrements count once, not trim every NULL')
    f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(0x413090 in p.visits,'actual material dtor delegates to physical NamedObject')
    for address in (0x74e060,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'native material/clone/pass allocations all released')
    print(f'PASS {checks}/{checks}: PC material {mode}; bounded original instructions, no GPU or PS2 assumptions')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
