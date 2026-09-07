#!/usr/bin/env python3
"""Original45F570 pass iteration→423460→467B70; bounded two UV stages."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup
class PassFixture(ApplyFixture):
    def observe(self,p,name,argc):
        args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
        if name!='transform' or args[0]!=self.device or args[1] not in (16,17):raise AssertionError('two-stage external UV boundary')
        if len(self.events)>=8:raise AssertionError('bounded UV observer')
        self.events.append([args[1],list(p.floats(args[2],16))]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)
def main(mode,return_capture=False):
    if mode not in ('empty','two-auto','two-stage0','two-stage1','eight-no-uv','failed'):raise ValueError('bounded pass update input')
    f=PassFixture(mode=='failed');p=f.p;owner=f.call(0x45f610);layers=[];visits=[];checks=0;maximum=0;captures=[]
    count=0 if mode=='empty' else 8 if mode=='eight-no-uv' else 2
    matrix=p.allocate(36)
    for index in range(count):
        layer=f.call(0x460e50);layers.append(layer);f.call(0x45f5e0,this=owner,args=(index,layer))
        if mode!='eight-no-uv':
            p.put_floats(matrix,tuple(float(v+index*10) for v in range(1,10)))
            f.call(0x467cb0,this=p.uint(layer+0x10),args=(matrix,))
    holders=[p.uint(l+0x10) for l in layers]
    def observe(machine,address,size,unused):
        if address==0x467b70:visits.append([holders.index(p.reg('ECX')),p.uint(p.reg('ESP')+4)])
    p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
    stage=0 if mode=='two-stage0' else 1 if mode=='two-stage1' else 0xffffffff
    for repeat in range(2):
        visits.clear();f.events.clear();f.call(0x45f570,this=owner,args=(stage,));maximum=max(maximum,sum(p.visits.values()))
        if not p.visits.get(0x4596b0):raise AssertionError('actual protected pass bridge resolves to4596B0')
        expected=[[i,i if stage==0xffffffff else stage] for i in range(count)]
        checks+=1
        if visits!=expected:raise AssertionError(('native layer stage order',visits,expected))
        captures.append([repeat,list(f.events)])
    cleanup(f,(owner,));checks+=1
    capture=[mode,captures]
    if not return_capture:print('MATERIAL_PASS_UPDATE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: actual pass update {mode}; maxInstructions={maximum}; heap={p.allocated}; bridge4596B0=true',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
