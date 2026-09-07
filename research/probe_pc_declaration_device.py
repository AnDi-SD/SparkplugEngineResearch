#!/usr/bin/env python3
"""Original PC declaration map miss/create/reuse/bind/clear with explicit COM seam."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_declaration_fixture import DeclarationDeviceFixture
from probe_pc_dx_buffers import check
import probe_pc_dx_buffers as counts

def main(mode):
    if mode not in {'map','declaration-failure'}:raise ValueError('explicit declaration mode required')
    f=DeclarationDeviceFixture(mode);p=f.p
    first=f.call(0x4ae0e0,this=f.renderer,args=(0x840,))
    print('DECL RESOLVE',mode,hex(first),'words',f.words(first,0x10,0x1c),
          'instructions',sum(p.visits.values()),'heap',p.allocated,'events',f.events,flush=True)
    check(first in f.allocations and p.uint(first)==0x6f2e58,'map miss creates actual PC declaration')
    check(p.uint(f.renderer+0xf360)==1,'native map insertion stores one declaration')
    event_count=len(f.events)
    check(f.call(0x4ae0e0,this=f.renderer,args=(0x840,))==first and len(f.events)==event_count,'repeat exact component mask reuses object without COM creation')
    check(p.uint(first+0x14)==0x112,'declaration base stores converted FVF')
    check(bool(p.uint(first+0x18))==(mode=='map'),'native failed Create still leaves cacheable declaration with null COM pointer')
    check(f.call(0x4c9d00,this=first,args=(0xdeadbeef,))&255==1,'bind uses own COM pointer; extra argument unused')
    f.clear_declarations()
    check(set(f.allocations)==set(f.freed),'all tracked declaration/map allocations freed')
    print(f'PASS {counts.checks}/{counts.checks}: original PC declaration {mode}; COM boundary, not GPU proof')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
