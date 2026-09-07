#!/usr/bin/env python3
"""Original4BC290 preselected-shader draw path/stack helpers; no GPU."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup
class DrawFixture(ApplyFixture):
    def __init__(self,fail=False):
        super().__init__(fail,renderer_size=0xf2f8,device_table_size=0x1b8);p=self.p;table=p.uint(self.device);self.tokens={0:0}
        for i,(slot,name,argc) in enumerate(((0x144,'draw',4),(0x148,'indexed',7),(0x178,'vertex-constants',4),(0x1b4,'pixel-constants',4))):
            address=0x34090e00+16*i;p.put_uint(table+slot,address);p.seams[address]=lambda machine,n=name,c=argc:self.observe(machine,n,c)
    def state(self):
        p=self.p;r=self.renderer
        return [self.tokens[p.uint(r+0xe44c)],self.tokens[p.uint(r+0xe450)],
            p.uint(r+0xe474),p.uint(r+0xe478),[self.tokens[p.uint(r+0xe454+4*i)] for i in range(4)],
            [self.tokens[p.uint(r+0xe464+4*i)] for i in range(4)]]
    def observe(self,p,name,argc):
        args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
        if args[0]!=self.device or len(self.events)>=16:raise AssertionError('bounded draw device')
        if name.endswith('-constants'):
            if args[1]!=0 or not 0<args[3]<=2:raise AssertionError('bounded constant registers')
            event=[name,args[1],[p.uint(args[2]+4*i) for i in range(4*args[3])],args[3]]
        elif name in ('vertex-shader','pixel-shader','draw','indexed'):event=[name,*args[1:]]
        else:raise AssertionError('unexpected draw external boundary')
        self.events.append([*event,self.state()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)
def main(mode,return_capture=False):
    if mode not in ('types','pixel','constants','failed','stacks'):raise ValueError('bounded preselected draw path')
    f=DrawFixture(mode=='failed');p=f.p;r=f.renderer;checks=0;maximum=0;captures=[]
    shaders=[]
    for ordinal in (1,2,3,4):
        obj=p.allocate(0x54);p.put_uint(obj+0x50,100+ordinal);f.tokens[obj]=ordinal;shaders.append(obj)
    p.put_uint(r+0xe454,shaders[0]);p.put_uint(r+0xe464,shaders[2])
    p.mu.mem_write(r+0xf2f5,bytes([int(mode in ('pixel','constants','failed'))]))
    if mode in ('constants','failed'):
        p.put_uint(r+0xe444,2);p.put_uint(r+0xe448,1)
        for i in range(8):p.put_uint(r+0xcbc4+4*i,0x3f000000+i)
        for i in range(4):p.put_uint(r+0xd804+4*i,0x40000000+i)
    if mode=='stacks':
        for entry,arg in ((0x4be1b0,shaders[1]),(0x4be1e0,shaders[3]),(0x4be1b0,0),(0x4be1d0,None),(0x4be200,None),(0x4be1d0,None)):
            f.call(entry,this=r,args=() if arg is None else (arg,));maximum=max(maximum,sum(p.visits.values()));checks+=1
            if f.events:raise AssertionError('stack helpers do not call device')
            captures.append([entry,0 if arg is None else f.tokens[arg],f.state()])
    else:
        for step,kind in enumerate((0,1,2,3,4,2,2)):
            if step==3:p.put_uint(r+0xe454,shaders[1])
            if step==5:p.put_uint(r+0xe464,shaders[3])
            if step==6:p.put_uint(r+0xe464,0)
            args=(kind,11,13,17,19,0x2019,23);f.events.clear();result=f.call(0x4bc290,this=r,args=args)&255
            maximum=max(maximum,sum(p.visits.values()));checks+=1
            if result!=1 or p.visits.get(0x4be310) or p.visits.get(0x4be2b0):raise AssertionError('preselected shader branch only; no fake automatic generation')
            captures.append([step,list(args),result,f.state(),list(f.events)])
    cleanup(f,());checks+=1
    capture=[mode,captures]
    if not return_capture:print('RENDERER_DRAW_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original preselected draw {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
