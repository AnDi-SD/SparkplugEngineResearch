#!/usr/bin/env python3
"""Independent copy437630 on declared objects; never calls capped factory.

Only NULL or pre-mapped material edges allowed, so recursive41AF70/41A580
cannot run. This tests the consumer body, NOT construction/virtual clone.
"""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_material_color import controller_input,OFFSETS
from probe_pc_function_eval import cleanup

def main(mode):
    if mode not in ('null','mapped'):raise ValueError('unmapped edge forbidden: reaches capped factory')
    f=PCWriteBytesFixture();p=f.p;animations=f.call(0x454640);source=controller_input(f);dest=controller_input(f);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    f.call(0x52fd90,this=0x755588);manager=f.call(0x412540);p.put_uint(0x74e060,manager);materials=[]
    if mode=='mapped':
        a=f.call(0x41a390);b=f.call(0x41a390);materials=[a,b];p.put_uint(source+0x24,a)
        f.call(0x412f70,this=manager,args=(a,b))
    p.put_floats(source+0x1c,(2,3));p.put_floats(source+0x28,tuple(float(i+1) for i in range(16)))
    for i,offset in enumerate(OFFSETS):
        p.put_uint(source+offset+0x10,0x12345678+i);p.put_uint(source+offset+0x14,0x87654321+i)
        p.put_floats(source+offset+0x28,(i+.5,2,.5,3,4,5,6,7));p.put_uint(source+offset+0x48,1);p.put_uint(source+offset+0x4c,8)
    p.put_floats(source+0x1a8+0x10,(21,22,23,24,25,26,27,28));p.put_uint(source+0x1dc,8)
    p.put_floats(dest+0x1a8+0x24,(91,));p.put_uint(dest+0x1dc,7);alpha_before=bytes(p.mu.mem_read(dest+0x1a8,0x38))
    result=f.call(0x437630,this=source,args=(dest,))&255;maximum=sum(p.visits.values())
    check(result==1,'actual copy succeeds independently of capped factory')
    check(p.uint(dest+0x24)==(materials[1] if materials else 0),'NULL/pre-mapped borrowed material')
    check(p.floats(dest+0x1c,2)==(2,3),'both render clocks copied')
    check(p.floats(dest+0x28,16)==p.floats(source+0x28,16),'saved four colors copied')
    for offset in OFFSETS:
        # Assignment copies clamp BYTE, not its three padding bytes. Source
        # test deliberately wrote uint32(1), so padding differs from CC input.
        check(bytes(p.mu.mem_read(dest+offset+0x10,8))==bytes(p.mu.mem_read(source+offset+0x10,8))
              and bytes(p.mu.mem_read(dest+offset+0x28,0x21))==bytes(p.mu.mem_read(source+offset+0x28,0x21))
              and p.uint(dest+offset+0x4c)==p.uint(source+offset+0x4c),'color endpoints and scalar nonpadding payload copied')
    check(bytes(p.mu.mem_read(dest+0x1a8,0x38))==alpha_before,'alpha evaluator wholly untouched: not copied/reset')
    check(bytes(p.mu.mem_read(dest+0x1a8,0x38))!=bytes(p.mu.mem_read(source+0x1a8,0x38)),'source alpha deliberately distinct')
    print('MATERIAL_COLOR_COPY',json.dumps([mode,list(p.floats(dest+0x1c,2)),list(p.floats(dest+0x28,16)),p.floats(dest+0x1cc,1)[0],p.uint(dest+0x1dc)]),flush=True)
    f.call(0x6d7db0)
    for material in materials:f.call(p.uint(p.uint(material)),this=material,args=(1,))
    f.call(0x4545d0,this=animations,args=(1,));cleanup(f,())
    check(set(f.allocations)==set(f.freed),'actual leaf/material/map/manager allocations freed')
    print(f'PASS {checks}/{checks}: declared-input material copy {mode}; maxInstructions={maximum}; heap={p.allocated}; factoryExcluded=true',flush=True);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
