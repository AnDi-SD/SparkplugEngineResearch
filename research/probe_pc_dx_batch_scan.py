#!/usr/bin/env python3
"""Original4AA870 first scan, stop BEFORE combiner allocation; never resume."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_read_reference import ReadReferenceFixture
from probe_pc_dx_mesh_metadata import HEADER,field,check
import probe_pc_dx_mesh_metadata as counts

def main(mode):
    cases={
      'same':([(0x112,10,320,60,0),(0x112,20,640,120,1)],(0x112,30,960,180)),
      'different':([(0x112,10,320,60,0),(0x142,20,640,120,0)],(0x112,10,320,60)),
      'strict-limit':([(0x112,10000,320000,60000,0),(0x112,10000,320000,60000,0)],(0x112,10000,320000,60000)),
      'missing-after':([(0x112,10,320,60,0),None],(0x112,20,640,120)),
      'missing-first':([None,(0x112,10,320,60,0)],(0,0,0,0)),
      'zero':([(0x112,0,0,0,0)],(0x112,0,0,0)),
      'too-large':([(0x112,20000,640000,120000,0)],None),
    }
    if mode not in cases:raise ValueError('explicit bounded scan mode required')
    inputs,expected=cases[mode]
    bodies=[HEADER+(field(v) if v else b'')+b'\0' for v in inputs]
    f=ReadReferenceFixture(b''.join(bodies),ids=tuple(7+i for i in range(len(bodies))));p=f.p
    p.put_uint(f.stream+0x14,f.directory_end)
    offset=0;entries=[]
    for i,body in enumerate(bodies):
        entry=f.call(0x4664c0,this=f.fat,args=(7+i,));entries.append(entry)
        p.put_uint(entry+0x10,0x33c34cf0);p.put_uint(entry+0x14,offset);p.put_uint(entry+0x18,len(body))
        offset+=len(body)
    p.run(0x4aa870,args=(f.fat,f.stream),stop_at=0x4aa9ad)
    actual=(p.uint(p.reg('ESP')+0x20),p.reg('EDI'),p.reg('EBP'),p.reg('EBX')) if p.reg('EIP')==0x4aa9ad else None
    print('SCAN',mode,'batch',actual,'eip',hex(p.reg('EIP')),'instructions',sum(p.visits.values()),flush=True)
    check(actual==expected,'original first scan aggregate including stale/missing outputs')
    if expected is None:check(p.reg('EAX')&255==1,'oversize-only first pass completes without a batch')
    else:check(p.reg('EIP')==0x4aa9ad,'intentional stop before allocation, no fake materialization')
    check(0x4a9610 not in p.visits and 0x4a96c0 not in p.visits,'combiner and GPU tail never entered')
    check(all(p.uint(e+0x20)==0 for e in entries),'no resource was falsely materialized')
    check(p.uint(0x763148)==0 and not f.errors,'no active combiner published or diagnostics')
    # Explicit fixture abort: stopped native frame is discarded, not resumed.
    p.put_uint(f.teb,0xffffffff);f.cleanup()
    print(f'PASS {counts.checks}/{counts.checks}: original DX first scan {mode}')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
