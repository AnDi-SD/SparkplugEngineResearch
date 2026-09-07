#!/usr/bin/env python3
"""Independent bounded PC vertex-shader factory scout; external COM only."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup

def main():
    f=ApplyFixture();p=f.p;refs=[]
    for slot in (4,8):
        address=0x34090e00+slot;p.put_uint(p.uint(f.device)+slot,address)
        def observe(machine,s=slot):
            if machine.uint(machine.reg('ESP')+4)!=f.device:raise AssertionError('declared device lifetime only')
            refs.append(s);machine.fixture_return(4,eax=1)
        p.seams[address]=observe
    obj=f.call(0x4c9f10);table=p.uint(obj)
    print('VERTEX_SHADER_FACTORY',hex(obj),hex(f.allocations[obj]),hex(table),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    print('FIELDS',[(hex(i),hex(p.uint(obj+i))) for i in range(0,f.allocations[obj],4)],flush=True)
    print('VTABLE',[hex(p.uint(table+4*i)) for i in range(10)],flush=True)
    record=f.call(p.uint(table+16),this=obj)
    print('RTTI',hex(record),'lifetime',refs,flush=True)
    cleanup(f,(obj,));print('PASS vertex factory/destruction',refs,flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
