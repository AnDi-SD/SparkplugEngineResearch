#!/usr/bin/env python3
"""Whole PC4BC4A0 bounded buffer/material/pass consumer, no GPU or internal seams."""
from pathlib import Path
import json,sys,struct
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup,bits

class SubmitFixture(ApplyFixture):
    def __init__(self,fail=False):
        super().__init__(fail,renderer_size=0xf2f8,device_table_size=0x1b8)
        self.prepare_submission_device()

    def prepare_submission_device(self):
        p=self.p;table=p.uint(self.device);self.tokens={0:0};self.buffers=[]
        for i,(slot,name,argc) in enumerate(((0x144,'draw',4),(0x148,'indexed',7),(0x178,'vertex-constants',4),(0x1b4,'pixel-constants',4),(0x15c,'declaration',2),(0x190,'stream',5),(0x1a0,'indices',2))):
            address=0x34090d00+16*i;p.put_uint(table+slot,address)
            p.seams[address]=lambda machine,n=name,c=argc:self.observe(machine,n,c)
    def state(self):
        p=self.p;r=self.renderer
        return [self.tokens[p.uint(r+offset)] for offset in (0xc9fc,0xca04,0xca08)]+[[p.uint(obj+8)&65535 for obj in self.buffers]]
    def observe(self,p,name,argc):
        args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
        if args[0]!=self.device or len(self.events)>=128:raise AssertionError('bounded submission COM observer')
        if name=='material':event=[name,[p.uint(args[1]+4*i) for i in range(17)]]
        elif name=='vertex-constants':
            if args[1]!=0 or args[3] not in (4,8):raise AssertionError('bounded weighted rows')
            event=[name,args[1],[p.uint(args[2]+4*i) for i in range(args[3]*4)],args[3]]
        elif name in ('render','stage','sampler','texture','vertex-shader','pixel-shader','declaration','stream','indices','draw','indexed'):event=[name,*args[1:]]
        else:raise AssertionError('unexpected automatic constant/transform branch')
        self.events.append([*event,self.state()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)

def main(mode,return_capture=False):
    if mode not in ('empty','one','two','failed','switch','mesh','weighted','weighted-two','weighted-failed','weighted-matrix','weighted-append','weighted-append-matrix'):raise ValueError('bounded unlit complete submit with no-weight or real cached shader')
    weighted=mode.startswith('weighted')
    matrix_mode=mode.endswith('matrix');produced='append' in mode
    f=SubmitFixture(mode in ('failed','weighted-failed'));p=f.p;r=f.renderer;checks=0;maximum=0;captures=[]
    def make_material(count,layers):
        material=f.call(0x4a9460);p.put_uint(material+0xb8,0);p.put_uint(material+0x38,2)
        for i in range(count):
            owner=f.call(0x45f610);f.call(0x423960,this=material,args=(i,owner));p.put_uint(owner+0x10,i*3)
            for j in range(layers):
                layer=f.call(0x460e50);f.call(0x45f5e0,this=owner,args=(j,layer))
        return material
    default=make_material(1,2);material=make_material(0 if mode=='empty' else 2 if mode in ('two','weighted-two') else 1,1)
    p.put_uint(r+0xc9c0,default);p.put_uint(r+0xc18c,material)
    debug=f.call(0x41e380);p.put_uint(0x75526c,debug)
    declarations=[f.call(0x4c9c20),f.call(0x4c9c20)]
    indices=[f.call(0x4b2010),f.call(0x4b2010)];vertices=[f.call(0x4b1e30),f.call(0x4b1e30)]
    f.buffers=indices+vertices
    for i,obj in enumerate(declarations+f.buffers):f.tokens[obj]=i+1
    # Actual factory NULL COM handles. Declared caller references keep buffers
    # alive across cache switches; original renderer increments/decrements run.
    for obj in f.buffers:p.mu.mem_write(obj+8,(1).to_bytes(2,'little'))
    mesh=p.allocate(0x88) if mode=='mesh' else 0
    matrices=0
    if weighted:
        manager=f.call(0x4c9680);p.put_uint(0x763024,manager);shader=f.call(0x4c9f10)
        specs=[(5,0,3),(8,3,1)]+([(1,4,4)] if matrix_mode else [])
        if produced:
            for kind,start,count in specs:
                name={5:b'BlendMatrices',8:b'MatDiffuse',1:b'view_proj_matrix'}[kind]
                words=struct.unpack('<11I',name.ljust(32,b'\0')+struct.pack('<III',0xdeadbeef,start,count))
                f.call(0x4af940,this=shader,args=words)
                if not p.visits.get(0x45a6c0):raise AssertionError('real protected descriptor producer')
            if p.uint(shader+0x34)!=(8 if matrix_mode else 4):raise AssertionError('actual producer row sum')
        else:
            p.put_uint(shader+0x34,8 if matrix_mode else 4)
            size=44*len(specs);p.run(0x412400,args=(size,),callee_pop=False);desc=p.reg('EAX');p.mu.mem_write(desc,bytes(size))
            for off,value in ((0x3c,desc),(0x40,desc+size),(0x44,desc+size)):p.put_uint(shader+off,value)
            for i,spec in enumerate(specs):
                for off,value in zip((0x20,0x24,0x28),spec):p.put_uint(desc+44*i+off,value)
        pair=p.allocate(12);output=p.allocate(4);p.mu.mem_write(pair,struct.pack('<III',0x20011,0,shader))
        f.call(0x4c87a0,this=manager+0x44,args=(output,p.uint(manager+0x48),pair))
        matrices=p.allocate(64);p.put_uint(r+0xc9b8,matrices)
    for iteration in range(4):
        if weighted:
            for i in range(16):p.put_uint(matrices+4*i,bits(float(iteration*100+i)))
            for i,value in enumerate((.25,.5,.75,float(iteration))):p.put_uint(material+0x78+4*i,bits(value))
            if matrix_mode:
                for m,off in enumerate((0xca40,0xca80,0xcac0)):
                    for i in range(16):p.put_uint(r+off+4*i,bits(float((i+m+iteration)%5-2)))
                p.mu.mem_write(r+0xf2f4,b'\1')
        selected=1 if mode=='switch' and iteration>=2 else 0
        ib=0 if mode=='switch' and iteration==3 else indices[selected]
        args=(ib,vertices[selected],1 if iteration==3 else 2,11,13,17,19,declarations[selected],32 if iteration<1 else 48,0x803 if weighted else 0x801)
        if mesh:
            for off,value in zip((0x54,0x58,0x50,0x78,0x48,0x7c,0x4c,0x84,0x74,0x44),args):p.put_uint(mesh+off,value)
        f.events.clear();result=f.call(0x4bc670 if mesh else 0x4bc4a0,this=r+0x18 if mesh else r,args=(mesh,) if mesh else args)&255
        maximum=max(maximum,sum(p.visits.values()));checks+=1
        if result!=1 or not p.visits.get(0x4be180) or not p.visits.get(0x4bb890) or p.visits.get(0x4bde50):raise AssertionError('complete unlit material submission path')
        if mode!='empty' and (not p.visits.get(0x45f570) or not p.visits.get(0x4bc410) or not p.visits.get(0x4bc290) or not p.visits.get(0x4c8980)):raise AssertionError('real pass/update/automatic no-weight draw')
        if weighted and (not p.visits.get(0x4ae930) or not p.visits.get(0x4be210) or p.visits.get(0x4c89eb) or p.uint(r+0xe454)):raise AssertionError('actual cached evaluation, no generating seam, auto selection cleared')
        if matrix_mode and not p.visits.get(0x4ad540):raise AssertionError('actual dirty refresh within full submission')
        captures.append([iteration,result,f.state(),[p.uint(r+0xc868+4*i) for i in range(11)],p.uint(material+0x34),list(f.events)])
    # Explicit teardown of declared consumer inputs, not renderer destructor.
    cleanup(f,tuple(obj for obj in (material,default,*declarations,*f.buffers,p.uint(0x763024)) if obj));checks+=1
    capture=[mode,captures]
    if not return_capture:print('RENDERER_SUBMIT_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original whole renderer submission {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
