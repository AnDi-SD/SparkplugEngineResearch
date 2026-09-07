#!/usr/bin/env python3
"""Actual weighted automatic4BC290 cache-hit path including4AE930/4BE210."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup,bits

class CachedDrawFixture(ApplyFixture):
    def __init__(self,fail=False):
        super().__init__(fail,renderer_size=0xf2f8,device_table_size=0x1b8);p=self.p;table=p.uint(self.device);self.tokens={0:0}
        for i,(slot,name,argc) in enumerate(((0x144,'draw',4),(0x148,'indexed',7),(0x178,'vertex-constants',4),(0x1b4,'pixel-constants',4))):
            address=0x34090b00+16*i;p.put_uint(table+slot,address)
            p.seams[address]=lambda machine,n=name,c=argc:self.observe(machine,n,c)
    def state(self):
        p=self.p;r=self.renderer
        return [self.tokens[p.uint(r+0xe454)],self.tokens[p.uint(r+0xe44c)],p.uint(r+0xe444),[p.uint(r+0xcbc4+4*i) for i in range(16)]]
    def observe(self,p,name,argc):
        args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
        if args[0]!=self.device or len(self.events)>=8:raise AssertionError('bounded weighted draw COM')
        if name=='vertex-constants':
            if args[1]!=0 or args[3]!=4:raise AssertionError('four fully populated shader rows only')
            event=[name,args[1],[p.uint(args[2]+4*i) for i in range(16)],args[3]]
        elif name in ('vertex-shader','draw','indexed'):event=[name,*args[1:]]
        else:raise AssertionError('unexpected pixel/other boundary')
        self.events.append([*event,self.state()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)

def main(mode,return_capture=False):
    if mode not in ('cached','failed','preselected'):raise ValueError('bounded existing shader, no compiler miss')
    f=CachedDrawFixture(mode=='failed');p=f.p;r=f.renderer;maximum=0;checks=0;captures=[]
    manager=f.call(0x4c9680);p.put_uint(0x763024,manager)
    material=f.call(0x4a9460);p.put_uint(material+0xb8,0);p.put_uint(r+0xe47c,material)
    shader=f.call(0x4c9f10);f.tokens[shader]=1;p.put_uint(shader+0x34,4)
    p.run(0x412400,args=(88,),callee_pop=False);descriptors=p.reg('EAX')
    p.mu.mem_write(descriptors,bytes(88));p.put_uint(shader+0x3c,descriptors);p.put_uint(shader+0x40,descriptors+88);p.put_uint(shader+0x44,descriptors+88)
    for i,(kind,start,count) in enumerate(((5,0,3),(8,3,1))):
        for off,value in ((0x20,kind),(0x24,start),(0x28,count)):p.put_uint(descriptors+44*i+off,value)
    pair=p.allocate(12);output=p.allocate(4);p.mu.mem_write(pair,struct.pack('<III',0x11,0,shader))
    f.call(0x4c87a0,this=manager+0x44,args=(output,p.uint(manager+0x48),pair))
    matrices=p.allocate(64);p.put_uint(r+0xc9b8,matrices)
    for iteration in range(4):
        for i in range(16):p.put_uint(matrices+4*i,bits(float(iteration*100+i)))
        for i,value in enumerate((.25,.5,.75,float(iteration))):p.put_uint(material+0x78+4*i,bits(value))
        if mode=='preselected' and iteration==1:p.put_uint(r+0xe454,shader)
        f.events.clear();result=f.call(0x4bc290,this=r,args=(1 if iteration==3 else 2,11,13,17,19,0x803,0))&255
        maximum=max(maximum,sum(p.visits.values()));checks+=1
        automatic=not(mode=='preselected' and iteration>=1)
        if result!=1 or bool(p.visits.get(0x4c8980))!=automatic or bool(p.visits.get(0x4ae930))!=automatic or bool(p.visits.get(0x4be210))!=automatic or p.visits.get(0x4c89eb):raise AssertionError('actual cached/no generating miss; preselection skips recalculation')
        captures.append([iteration,result,f.state(),list(f.events)])
    cleanup(f,(material,manager));checks+=1;capture=[mode,captures]
    if not return_capture:print('RENDERER_CACHED_DRAW_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original cached automatic draw {mode}; maxInstructions={maximum};heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
