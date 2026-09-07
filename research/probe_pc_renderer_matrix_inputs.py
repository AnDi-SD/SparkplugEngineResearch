#!/usr/bin/env python3
"""Original renderer matrix setters/getters, external SetTransform observer."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup

class MatrixFixture(ApplyFixture):
    def __init__(self,fail=False):super().__init__(fail,renderer_size=0xf2f8)
    def state(self):
        p=self.p;r=self.renderer
        return [int.from_bytes(p.mu.mem_read(r+0xf2f4,1),'little'),[[p.uint(r+off+4*i) for i in range(16)] for off in (0xca40,0xca80,0xcac0)]]
    def observe(self,p,name,argc):
        args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
        if name!='transform' or args[0]!=self.device or len(self.events)>=1:raise AssertionError('only original SetTransform boundary')
        self.events.append([args[1],[p.uint(args[2]+4*i) for i in range(16)],self.state()])
        p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)

def main(mode,return_capture=False):
    if mode not in ('world','view','projection','failed','alias'):raise ValueError('bounded matrix inputs')
    f=MatrixFixture(mode=='failed');p=f.p;r=f.renderer;checks=0;maximum=0;captures=[];source=p.allocate(64)
    for ordinal,index in enumerate((0,1,2,0,1,2)):
        actual=index if mode in ('failed','alias') else {'world':0,'view':1,'projection':2}[mode]
        for i in range(16):p.put_uint(source+4*i,(0x3f000000+0x12345*i) if ordinal<3 else (0x7fc00000 if i==0 else 0xff000000+i))
        ptr=r+(0xca40,0xca80,0xcac0)[actual] if mode=='alias' else source
        p.mu.mem_write(r+0xf2f4,b'\0');f.events.clear()
        result=f.call((0x4bbb60,0x4bbb20,0x4bbae0)[actual],this=r+0x18,args=(ptr,0xdeadbeef) if actual==0 else (ptr,))&255
        maximum=max(maximum,sum(p.visits.values()));checks+=1
        if result!=1 or len(f.events)!=1 or f.events[0][0]!=(256,2,3)[actual]:raise AssertionError('original always-submit/ignored world second arg')
        getter=f.call((0x4ad350,0x4ad360,0x4ad370)[actual],this=r+0x18)
        if getter!=r+(0xca40,0xca80,0xcac0)[actual] or p.visits.get(0x4ad540):raise AssertionError('raw input getter never refreshes')
        captures.append([ordinal,actual,result,f.state(),list(f.events)])
    cleanup(f,());checks+=1;capture=[mode,captures]
    if not return_capture:print('RENDERER_MATRIX_INPUTS_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original matrix inputs {mode}; maxInstructions={maximum};heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
