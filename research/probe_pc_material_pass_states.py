#!/usr/bin/env python3
"""Actual PC4BBBA0/4BC410 pass texture states and final blend, no GPU."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup
class PassStateFixture(ApplyFixture):
    def observe(self,p,name,argc):
        super().observe(p,name,argc)
        if name not in ('texture','stage','sampler','render'):raise AssertionError('pass state boundary')
        r=self.renderer
        self.events[-1].append([p.uint(r+0xc898+4*i) for i in range(72)])
def main(mode,return_capture=False):
    if mode not in ('empty','one','two','override','shader-override','failed','blend','blend-override'):raise ValueError('bounded pass state case')
    f=PassStateFixture(mode=='failed');p=f.p;r=f.renderer;checks=0;maximum=0;captures=[]
    def make_material(count):
        material=f.call(0x4a9460);owner=f.call(0x45f610);f.call(0x423960,this=material,args=(0,owner));layers=[]
        for i in range(count):
            layer=f.call(0x460e50);f.call(0x45f5e0,this=owner,args=(i,layer));layers.append(layer)
        return material,owner,layers
    default,default_pass,default_layers=make_material(2)
    material,owner,layers=make_material(0 if mode=='empty' else 1 if mode=='one' else 2)
    p.put_uint(r+0xc9c0,default);p.put_uint(r+0xc18c,material);p.put_uint(r+0xc1c8,material+0x18)
    debug=f.call(0x41e380);p.put_uint(0x75526c,debug)
    presets=[[0,1,2,0,1,0x11223344,1,0,2],[0,0,0,2,2,0,0,0,0],
             [0,3,4,1,0,0x55667788,2,1,3],[0,5,6,0,1,0x12345678,3,9,0x102]]
    for layer,values in zip(default_layers+layers,presets):
        for i,v in enumerate(values):p.put_uint(p.uint(layer+0x10)+0x10+4*i,v)
    # Force cleanup of old desired/bound NULL stages without fake texture objects.
    for stage in range(8):
        p.put_uint(r+0xc198+4*stage,0x12340000+stage);p.put_uint(r+0xe480+4*stage,0x12340000+stage)
        for index in range(9):p.put_uint(r+0xc898+4*(9*stage+index),0xcccccccc)
        p.put_uint(r+0xe920+0x100*stage,0xeeeeeeee);p.put_uint(r+0xe954+0x100*stage,0xdddddddd)
    if mode=='shader-override':p.put_uint(r+0xe454,0x12345678) # only tested non-NULL consumer flag here, never dereferenced
    if mode=='override':
        # One explicit alternative texture-state block at C3BC, selectors per stage/state.
        for stage in range(8):
            for index in range(9):
                p.put_uint(r+0xc29c+4*(72+9*stage+index),presets[2][index])
                p.put_uint(r+0xc748+4*(9*stage+index),index%2)
    if mode=='blend-override':
        alternate=p.allocate(44);p.put_uint(alternate+7*4,6)
        p.put_uint(r+0xc1c4,1);p.put_uint(r+0xc1cc,alternate);p.put_uint(r+0xc738,1)
    for iteration in range(3):
        f.events.clear()
        if iteration==2 and layers:p.put_uint(p.uint(layers[0]+0x10)+0x10+4*3,2)
        if mode.startswith('blend'):p.put_uint(owner+0x10,(0,3,6)[iteration])
        if mode=='blend-override' and iteration==2:p.put_uint(alternate+7*4,3)
        entry=0x4bc410 if mode.startswith('blend') else 0x4bbba0
        result=f.call(entry,this=r,args=(0,))&255;maximum=max(maximum,sum(p.visits.values()));checks+=1
        if result!=1:raise AssertionError('original finite pass state success')
        captures.append([iteration,result,[p.uint(r+0xc29c+4*i) for i in range(72)],
            [p.uint(r+0xc898+4*i) for i in range(72)],
            [p.uint(r+0xe920+0x100*i) for i in range(8)],
            [p.uint(r+0xe954+0x100*i) for i in range(8)],
            list(bytes(p.mu.mem_read(r+0xc1b8,8))),p.uint(material+0x18+7*4),p.uint(r+0xc884),list(f.events)])
    cleanup(f,(material,default));checks+=1
    capture=[mode,captures]
    if not return_capture:print('MATERIAL_PASS_STATES_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original pass states {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
