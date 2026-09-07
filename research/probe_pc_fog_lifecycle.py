#!/usr/bin/env python3
"""Original Fog blank clone and inherited copy with actual native clone map."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

def main():
    checks=0
    def check(value,label):
        nonlocal checks
        checks+=1
        if not value:raise AssertionError(label)
    f=PCFileBytesFixture(b'');p=f.p
    f.call(0x52fd90,this=0x755588) # actual clone-map initializer, no atexit
    source=f.call(0x419e90);before=bytes(p.mu.mem_read(source+0x14,20))
    changed=struct.pack('<IIfff',3,0x12345678,-2,123,.25);p.mu.mem_write(source+0x14,changed)
    clone=f.call(0x41a8e0,this=source)
    print('FOG CLONE',hex(clone),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    check(clone!=source and f.allocations.get(clone)==0x28,'actual distinct Fog clone allocation28')
    check(bytes(p.mu.mem_read(clone+0x14,20))==before,'native clone is blank/default, NOT source payload copy')
    check(bytes(p.mu.mem_read(source+0x14,20))==changed,'clone preserves source payload')
    check(0x412f70 in p.visits,'actual native clone-pair map registration, not seam')
    # Native inherited primary slot0C copy returns true without Fog payload copy.
    copy=p.uint(p.uint(source)+0xc);result=f.call(copy,this=source,args=(clone,))&255
    check(result==1 and bytes(p.mu.mem_read(clone+0x14,20))==before,'inherited base copy leaves Fog payload untouched')
    f.call(p.uint(p.uint(clone)),this=clone,args=(1,));f.call(p.uint(p.uint(source)),this=source,args=(1,));f.call(0x6d7db0)
    # Clone creates its separately owned manager74E060 in addition to the
    # static map. The initial scout omitted that singleton teardown.
    for address in (0x74e060,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    remaining=[(hex(a),size,hex(p.uint(a))) for a,size in f.allocations.items() if a not in f.freed]
    if remaining:print('FOG CLONE REMAINING',remaining,flush=True)
    check(set(f.allocations)==set(f.freed),'actual Fog/map storage released')
    print(f'PASS {checks}/{checks}: Fog blank clone/inherited copy/native map')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
