#!/usr/bin/env python3
"""Original PC4BB1F0 texture-state table translation; declared COM observers."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup

def case_inputs(mode):
    if mode=='ops':return [(i,v) for i in (1,2) for v in range(16)]
    if mode=='address':return [(i,v) for i in (3,4) for v in (0,1,2,3,0xffffffff)]
    if mode=='border':return [(5,v) for v in (0,0xff000000,0x12345678,0xffffffff)]
    if mode=='filter':return [(6,v) for v in (0,1,2,3,4,0xffffffff)]
    if mode in ('uv-index','hardware'):return [(7,v) for v in (*range(14),0xffffffff)]
    if mode=='uv-flags':return [(8,v) for v in (*range(17),0xffffffff)]
    if mode=='failed':return [(i,v) for i in range(1,9) for v in (0,1,2,3)]
    if mode=='hardware-flags':return [(8,v) for v in (*range(17),0xffffffff)]
    raise ValueError('bounded texture enum group')

class TextureFixture(ApplyFixture):
    def __init__(self,fail=False):
        self.active_state=0;self.active_stage=0
        super().__init__(fail)
    def observe(self,p,name,argc):
        super().observe(p,name,argc)
        raw=p.uint(self.renderer+0xc898+4*(9*self.active_stage+self.active_state))
        if name not in ('stage','sampler'):raise AssertionError('unexpected internal boundary outside texture map')
        self.events[-1].extend([raw,p.uint(self.renderer+0xe920+256*self.active_stage),p.uint(self.renderer+0xe954+256*self.active_stage)])

def main(mode,return_capture=False):
    inputs=case_inputs(mode);f=TextureFixture(mode=='failed');p=f.p;checks=0;maximum=0;captures=[]
    if mode.startswith('hardware'):
        p.put_uint(f.renderer+0xe474,3);p.put_uint(f.renderer+0xe454+12,1)
    for stage in (0,1,7):
        for index,value in inputs:
            f.active_stage=stage;f.active_state=index
            for repeat in range(2):
                f.events.clear();result=f.call(0x4bb1f0,this=f.renderer,args=(stage,index,value))&255;maximum=max(maximum,sum(p.visits.values()))
                raw=p.uint(f.renderer+0xc898+4*(9*stage+index));uv=p.uint(f.renderer+0xe920+256*stage);flags=p.uint(f.renderer+0xe954+256*stage)
                checks+=1
                if result!=1 or raw!=(stage if mode=='hardware' else value):raise AssertionError('actual successful texture state and raw cache / hardware override')
                if repeat:
                    checks+=1
                    expected=0 if index in (7,8) else 3 if index==6 else 1
                    if len(f.events)!=expected:raise AssertionError('only UV states deduplicate; sampler/op/border always submit')
                captures.append([stage,index,value,result,raw,uv,flags,list(f.events)])
    cleanup(f,());checks+=1
    if set(f.allocations)!=set(f.freed):raise AssertionError('native cleanup')
    capture=[mode,captures]
    if not return_capture:print('TEXTURE_STATE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original texture map {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
