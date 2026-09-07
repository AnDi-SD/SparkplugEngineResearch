#!/usr/bin/env python3
"""Original4BB650 texture identity cache and4BB1C0 palette selection; no GPU."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup

class BindingFixture(ApplyFixture):
    def __init__(self,fail=False):
        super().__init__(fail);p=self.p;table=p.uint(self.device);self.refs=1;self.texture_refs={};self.identities={0:0};self.handles={0:0};self.stage=0
        for slot,address,callback in ((4,0x340900a0,self.addref),(8,0x340900b0,self.release),(0x124,0x340900c0,self.palette)):
            p.put_uint(table+slot,address);p.seams[address]=callback
        p.seams[0x340900d0]=self.release_texture
    def addref(self,p):
        if p.uint(p.reg('ESP')+4)!=self.device:raise AssertionError('device AddRef identity')
        self.refs+=1;p.fixture_return(4,eax=self.refs)
    def release(self,p):
        if p.uint(p.reg('ESP')+4)!=self.device or self.refs<=1:raise AssertionError('device Release balance')
        self.refs-=1;p.fixture_return(4,eax=self.refs)
    def release_texture(self,p):
        obj=p.uint(p.reg('ESP')+4)
        if self.texture_refs.get(obj)!=1:raise AssertionError('declared external texture owner release')
        self.texture_refs[obj]=0;p.fixture_return(4,eax=0)
    def create_texture_input(self,ordinal):
        p=self.p;obj=self.call(0x4ab520);self.identities[obj]=ordinal
        handle=p.allocate(4);table=p.allocate(12);p.put_uint(handle,table);p.put_uint(table+8,0x340900d0)
        p.put_uint(obj+0x3c,handle);self.handles[handle]=ordinal;self.texture_refs[handle]=1
        return obj
    def snapshot(self):
        p=self.p;return [[self.identities[p.uint(self.renderer+0xe480+4*i)] for i in (0,1,7)],p.uint(self.renderer+0xca0c)]
    def palette(self,p):
        sp=p.reg('ESP')
        if p.uint(sp+4)!=self.device:raise AssertionError('palette device identity')
        self.events.append(['palette',p.uint(sp+8),self.snapshot()]);p.fixture_return(8,eax=0x80004005 if self.fail else 0)
    def observe(self,p,name,argc):
        args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
        if name!='texture' or args[0]!=self.device or args[1] not in (0,1,7):raise AssertionError('bounded texture COM call')
        self.events.append(['texture',args[1],self.handles[args[2]],self.snapshot()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)

def main(mode,return_capture=False):
    if mode not in ('plain','palette','failed','debug','palette-direct'):raise ValueError('bounded texture binding case')
    f=BindingFixture(mode=='failed');p=f.p;r=f.renderer;checks=0;maximum=0;captures=[]
    # Original known RTTI chain, explicit static-startup input, not factory discovery.
    for record,identity,parent in ((0x763210,0x3f3651b6,0x75df10),(0x75df10,0x2f281e13,0x7555f8),(0x7555f8,0x44de07fd,0x755310),(0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    first=f.create_texture_input(1);second=f.create_texture_input(2);palette=f.call(0x4b2c80)
    if mode in ('palette','failed'):f.call(0x4b93c0,this=first,args=(palette,))
    p.put_uint(palette+0x10,7);p.put_uint(r+0xca0c,0xffffffff)
    debug=f.call(0x41e380);p.put_uint(0x75526c,debug)
    inputs=[(0,0),(0,1),(0,1),(1,1),(7,2),(0,2),(0,0),(0,0),(1,0),(7,0)]
    if mode=='palette-direct':inputs=[(0,v) for v in (1,1,0,2,2,0)]
    for step,(stage,ordinal) in enumerate(inputs):
        if mode=='debug':p.mu.mem_write(debug+0x1f,bytes([int(2<=step<6)]))
        f.events.clear()
        if mode=='palette-direct':
            if ordinal:p.put_uint(palette+0x10,7 if ordinal==1 else 3)
            result=f.call(0x4bb1c0,this=r,args=(palette if ordinal else 0,))&255
        else:result=f.call(0x4bb650,this=r,args=((0,first,second)[ordinal],stage))&255
        maximum=max(maximum,sum(p.visits.values()));checks+=1
        if result!=1:raise AssertionError('original binding wrappers ignore HRESULT')
        captures.append([step,stage,ordinal,result,f.snapshot(),list(f.events)])
    for obj in (first,second,palette):f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    resource_manager=p.uint(0x75db78)
    if resource_manager:f.call(p.uint(p.uint(resource_manager)),this=resource_manager,args=(1,))
    cleanup(f,());checks+=1
    if f.refs!=1 or any(f.texture_refs.values()):raise AssertionError('actual texture/device teardown balanced')
    capture=[mode,captures]
    if not return_capture:print('TEXTURE_BINDING_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original texture binding {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
