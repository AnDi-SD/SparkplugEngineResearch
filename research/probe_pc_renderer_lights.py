#!/usr/bin/env python3
"""Actual PC4BDE50 light submission/cache consumer, external COM only."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup,bits

class LightsFixture(ApplyFixture):
    def __init__(self,fail=False):
        super().__init__(fail,renderer_size=0xf358);p=self.p;table=p.uint(self.device);self.tokens={0:0}
        for i,(slot,name,argc) in enumerate(((0xcc,'light',3),(0xd4,'enable',3))):
            address=0x34090c00+16*i;p.put_uint(table+slot,address)
            p.seams[address]=lambda machine,n=name,c=argc:self.observe(machine,n,c)
    def state(self):
        p=self.p;r=self.renderer
        return [p.uint(0x764340),self.tokens[p.uint(r+0xc190)],[self.tokens[p.uint(r+0xf2f8+4*i)] for i in range(24)],[p.uint(r+0xc178+4*i) for i in range(4)],p.uint(r+0xe4f4+139*4)]
    def observe(self,p,name,argc):
        args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
        if args[0]!=self.device or len(self.events)>=32:raise AssertionError('bounded light observer')
        if name=='light':event=[name,args[1],[p.uint(args[2]+4*i) for i in range(26)]]
        elif name in ('enable','render'):event=[name,*args[1:]]
        else:raise AssertionError('unexpected light boundary')
        self.events.append([*event,self.state()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)

def main(mode,return_capture=False):
    if mode not in ('list','failed','ambient','fallback','tolerance','null'):raise ValueError('finite light consumer')
    f=LightsFixture(mode=='failed');p=f.p;r=f.renderer;maximum=0;checks=0;captures=[]
    lights=p.allocate(0x28);f.tokens[lights]=9
    objects=[p.allocate(0x158) for _ in range(3)]
    for i,obj in enumerate(objects):
        f.tokens[obj]=i+1;p.put_uint(lights+4*i,obj);p.put_uint(obj+0xc0,(2,0,1)[i]);p.mu.mem_write(obj+0xed,bytes([i!=1]))
        for j in range(26):p.put_uint(obj+0xf0+4*j,0x10000000+i*256+j)
        for j,value in enumerate((.25,.5,.75,1.)):p.put_uint(obj+0xc4+4*j,bits(value))
    p.put_uint(0x764340,5);p.put_uint(r+0xc190,lights);p.put_uint(0x73fe98,0x7f234567)
    # Non-NULL4BDE50 never replaces C190; caller normally already installed it.
    if mode in ('ambient','tolerance'):p.put_uint(lights+0x20,objects[0])
    if mode=='null':
        for i in range(24):p.put_uint(r+0xf2f8+4*i,objects[i%3])
        for i in range(4):p.put_uint(r+0xc178+4*i,bits(.75))
    for iteration in range(4):
        p.put_uint(lights+0x24,(3,3,1,0)[iteration])
        if mode=='fallback' and iteration==2:p.put_uint(0x73fe98,0xff123456)
        if mode=='ambient' and iteration==2:p.mu.mem_write(objects[0]+0xed,b'\0')
        if mode=='tolerance':
            p.put_uint(objects[0]+0xc4,bits((.25,.2505,.2511,.2511)[iteration]))
        f.events.clear();result=f.call(0x4bde50,this=r,args=(0 if mode=='null' else lights,))&255
        maximum=max(maximum,sum(p.visits.values()));checks+=1
        if result!=1:raise AssertionError('native ignored device HRESULT')
        captures.append([iteration,result,f.state(),list(f.events)])
    cleanup(f,());checks+=1;capture=[mode,captures]
    if not return_capture:print('RENDERER_LIGHTS_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original lights {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
