#!/usr/bin/env python3
"""Original PC material state8 /4BDB10 and color-source cache; no GPU."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup

def state_at(p,r):return [[p.uint(r+0xe4a4+4*i) for i in range(17)],p.uint(r+0xe4e8),p.uint(r+0xe4ec)]

class LightingFixture(ApplyFixture):
    def observe(self,p,name,argc):
        super().observe(p,name,argc)
        if name!='render':raise AssertionError('unexpected lighting device boundary')
        self.events[-1].append(state_at(p,self.renderer))

def main(mode,return_capture=False):
    if mode not in ('modes','failed','power-zero','power-negative','power-nan','power-inf','sources'):raise ValueError('bounded lighting input')
    f=LightingFixture(mode=='failed');p=f.p;checks=0;maximum=0;captures=[]
    r=f.renderer
    p.put_uint(r+0xc194,0x80402010)
    p.put_floats(r+0xe4a4,tuple((i+1)/16 for i in range(16)))
    power={'power-zero':0,'power-negative':0xbf800000,'power-nan':0x7fc12345,'power-inf':0x7f800000}.get(mode,0x41000000)
    p.put_uint(r+0xe4e4,power);p.put_uint(r+0xe4e8,77);p.put_uint(r+0xe4ec,88)
    for i in range(256):p.put_uint(r+0xe4f4+4*i,0xa5a5a5a5) # explicit existing cache, not constructor default
    def state():return state_at(p,r)
    inputs=[(entry,value) for entry in (0x4bddb0,0x4bde00) for value in (10,10,11,11,12,12,9,13,0xffffffff)] if mode=='sources' else [(0x4b0ad0,value) for value in (0,1,2,3,4,5,6,7,8,0xffffffff,6,1,3)]
    for entry,value in inputs:
        f.events.clear();before=state()
        if mode=='sources':result=f.call(entry,this=r,args=(value,))&255
        else:
            f.active_index=8;result=f.call(entry,this=r,args=(8,value))&255
        maximum=max(maximum,sum(p.visits.values()));checks+=1
        if result!=1:raise AssertionError('actual lighting/source wrappers always return1 for bounded input')
        captures.append([entry,value,result,before,state(),list(f.events)])
    cleanup(f,());checks+=1
    if set(f.allocations)!=set(f.freed):raise AssertionError('native lifetime cleanup')
    capture=[mode,captures]
    if not return_capture:print('MATERIAL_LIGHTING_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original material lighting {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
