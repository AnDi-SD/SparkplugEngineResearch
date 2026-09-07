#!/usr/bin/env python3
"""Actual4A9530 DX color frame/clock gate with declared renderer/controller.

No full EngineCore factory, capped controller factory, or GPU calls. Native
intrusive input pin keeps declared controller away from an unknown destructor.
"""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_material_color import controller_input,OFFSETS
from probe_pc_function_eval import cleanup,configure_floor,bits

def main(mode,return_capture=False):
    if mode not in ('none','constant','linear','random','shared'):raise ValueError('five bounded DX frame consumers')
    f=PCWriteBytesFixture();p=f.p;configure_floor(f);animations=f.call(0x454640);controller=controller_input(f)
    p.mu.mem_write(controller+8,b'\x01\0') # declared external input pin, not native ownership construction
    renderer=p.allocate(0x44);p.put_uint(0x75db68,renderer);material=f.call(0x4a9460);materials=[material];checks=0;states=[];maximum=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    if mode!='none':
        f.call(0x423a50,this=material,args=(controller,));check(p.uint(material+0x74)==controller and (p.uint(controller+8)&65535)==2,'actual setter retains one beyond declared external pin')
    if mode=='shared':
        materials.append(f.call(0x4a9460));f.call(0x423a50,this=materials[-1],args=(controller,))
        check(p.uint(controller+0x24)==materials[-1] and (p.uint(controller+8)&65535)==3,'shared last-bound material plus two retained edges')
    for i,offset in enumerate(OFFSETS):
        p.put_uint(controller+offset+0x10,0x80402010+i);p.put_uint(controller+offset+0x14,0xff112244+i)
        p.put_uint(controller+offset+0x4c,6 if mode=='random' else 7);p.put_floats(controller+offset+0x3c,(i*.25-.5,))
    p.put_uint(controller+0x1dc,6 if mode=='random' else 8 if mode=='linear' else 7)
    p.put_floats(controller+0x1cc,(0 if mode=='linear' else .25,));p.put_floats(controller+0x1d0,(1,))
    for frame,delta,force in ((0,.25,0),(0,.5,0),(0,0,1),(1,.25,0),(1,0,1),(2,0,0)):
        p.put_uint(renderer+0x40,frame);f.call(0x423190,this=controller,args=(bits(delta),));calls=[]
        for holder in materials:
            f.call(0x4a9530,this=holder+0x14,args=(force,));maximum=max(maximum,sum(p.visits.values()));calls.append(p.visits.get(0x4373e0,0)>0)
            check(p.uint(holder+0x70)==frame,'DX stamp stored even without controller evaluation')
        states.append([frame,delta,force,calls,[p.uint(h+0x70) for h in materials],[[list(p.floats(h+offset,4)) for offset in (0x88,0x78,0x98,0xa8)] for h in materials],list(p.floats(controller+0x1c,2))])
    for holder in materials:
        f.call(0x423a50,this=holder,args=(0,));check(p.uint(holder+0x74)==0,'actual NULL setter clears edge without binder')
        f.call(p.uint(p.uint(holder)),this=holder,args=(1,))
    check((p.uint(controller+8)&65535)==1,'declared external pin survives; fake controller destructor never executes')
    f.call(0x4545d0,this=animations,args=(1,));cleanup(f,());check(set(f.allocations)==set(f.freed),'actual DX materials/leaf/manager allocations freed')
    capture=[mode,states];print('MATERIAL_COLOR_FRAME_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: actual DX frame consumer {mode}; maxInstructions={maximum}; heap={p.allocated}; factoryExcluded=true',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
