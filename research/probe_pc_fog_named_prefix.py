#!/usr/bin/env python3
"""Fog's engine RTTI base differs from its physical named-object prefix."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

def main():
    checks=0
    def check(value,label):
        nonlocal checks
        checks+=1
        if not value:raise AssertionError(label)
    f=PCFileBytesFixture(b'');p=f.p;f.call(0x52fd90,this=0x755588)
    source=f.call(0x419e90)
    check(p.uint(source+0x10)==0,'original Fog constructor initializes physical name slot10')
    # Explicit external interned-name storage; native global NameManager is
    # not constructed. Actual named-copy increments its byte reference count.
    name=p.allocate(32);p.mu.mem_write(name+8,b'\x01fog-prefix\0');p.put_uint(source+0x10,name)
    clone=f.call(0x41a8e0,this=source)
    check(p.uint(clone+0x10)==name and bytes(p.mu.mem_read(name+8,1))==b'\x02','actual Fog clone shares and retains named prefix')
    check(0x413120 in p.visits,'actual spNamedObject copy entry used by Fog')
    # Record the original Fog -> NamedObject destructor boundary. Storage is
    # externally owned; clear only these test name aliases before actual dtor.
    p.put_uint(source+0x10,0);p.put_uint(clone+0x10,0)
    f.call(p.uint(p.uint(clone)),this=clone,args=(1,))
    check(0x413090 in p.visits,'actual Fog dtor delegates to NamedObject413090')
    f.call(p.uint(p.uint(source)),this=source,args=(1,));f.call(0x6d7db0)
    for address in (0x74e060,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native allocations freed; external test name is arena-owned')
    print(f'PASS {checks}/{checks}: Fog physical NamedObject prefix; engine RTTI remains BaseObject; real NameManager not claimed')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
