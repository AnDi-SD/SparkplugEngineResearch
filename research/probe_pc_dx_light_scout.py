#!/usr/bin/env python3
"""Bounded original DXLight factory scouting; no invented ctor layout."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup
def main():
    f=ApplyFixture();p=f.p;obj=f.call(0x4ac000);maximum=sum(p.visits.values())
    print('DX_LIGHT_FACTORY',json.dumps({'size':f.allocations[obj],'vtable':hex(p.uint(obj)),
        'slots':[hex(p.uint(p.uint(obj)+4*i)) for i in range(15)],
        'secondary':hex(p.uint(obj+0x10)),
        'words':[[hex(i),hex(p.uint(obj+i))] for i in range(0xb8,f.allocations[obj],4)]}),flush=True)
    print('FACTORY_VISITED',json.dumps([hex(a) for a in p.visits if 0x400000<=a<0x600000]),flush=True)
    cleanup(f,(obj,));print(f'PASS DXLight factory/cleanup; instructions={maximum};heap={p.allocated}',flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
