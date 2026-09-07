#!/usr/bin/env python3
"""Actual PC spPalette factory/layout/destructor; no renderer palette manager."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

def main():
    f=PCFileBytesFixture(b'');p=f.p;checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    obj=f.call(0x4b2c80);vt=p.uint(obj)
    print('PALETTE_FACTORY',hex(obj),'size',hex(f.allocations[obj]),'vtable',hex(vt),'slots',[hex(p.uint(vt+i)) for i in range(0,28,4)],
        'fields',[hex(p.uint(obj+i)) for i in range(0,0x24,4)],'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    check(f.allocations[obj]==0x414,'actual palette complete allocation414')
    check(f.call(p.uint(vt+0x10),this=obj)==0x763de0,'actual spPalette registration getter')
    check(p.uint(obj+0x10)==0xffffffff,'constructor palette slot/index sentinel minus1')
    check(bytes(p.mu.mem_read(obj+0x14,0x400))==b'\xcc'*0x400,'palette entries remain native constructor-uninitialized')
    f.call(p.uint(vt),this=obj,args=(1,))
    for address in (0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'all original palette allocations freed')
    print(f'PASS {checks}/{checks}: actual PC spPalette factory/getter/dtor subset; renderer is a separate probe')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
