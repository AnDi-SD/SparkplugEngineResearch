#!/usr/bin/env python3
"""Bounded original parser/RFX factory scout; no file or compiler execution."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup
from pc_stl_fixtures import install_char_traits

def main(mode):
    f=PCWriteBytesFixture();p=f.p
    if mode=='rfx':install_char_traits(p)
    entry={'parser':0x4d0da0,'rfx':0x4d54b0}[mode]
    obj=f.call(entry);size=f.allocations[obj];table=p.uint(obj)
    print('FACTORY',mode,hex(obj),hex(size),hex(table),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    print('FIELDS',[(hex(i),hex(p.uint(obj+i))) for i in (*range(0,0x28,4),*range(0x120,0x138,4),*range(0x538,size,4))],flush=True)
    print('DELIMITERS',[(i,chr(i)) for i in range(255) if bytes(p.mu.mem_read(obj+0x24+i,1))[0]],flush=True)
    print('VTABLE',[hex(p.uint(table+4*i)) for i in range(9)],flush=True)
    print('RTTI',hex(f.call(p.uint(table+16),this=obj)),flush=True)
    cleanup(f,(obj,));print('PASS original parser factory/lifetime',mode,flush=True);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
