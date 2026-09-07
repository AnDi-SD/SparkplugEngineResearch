#!/usr/bin/env python3
"""Independent PC material-color consumers with declared controller storage.

DO NOT call/resume capped41A580. This is NOT factory/whole-loader evidence.
Consumer-derived1E0 backing uses copied actual leaf factory states. No internal
runtime/codec/material callback is substituted, and no fake native owner dies.
"""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup,configure_floor,bits
from probe_pc_node_serializer import field

OFFSETS=(0x68,0xb8,0x108,0x158)

def controller_input(f):
    def forbidden(_):raise AssertionError('Capped material color factory/clone disabled before entry')
    f.p.seams[0x41a580]=forbidden;f.p.seams[0x41af70]=forbidden
    p=f.p;controller=p.allocate(0x1e0);p.put_uint(controller,0x6debd4)
    color=f.call(0x478a60);function=f.call(0x478840)
    for offset in OFFSETS:p.mu.mem_write(controller+offset,bytes(p.mu.mem_read(color,0x50)))
    p.mu.mem_write(controller+0x1a8,bytes(p.mu.mem_read(function,0x38)))
    # Actual temporaries are independent; embedded copies contain no owned refs.
    f.call(p.uint(p.uint(color)),this=color,args=(1,));f.call(p.uint(p.uint(function)),this=function,args=(1,))
    return controller

def main(mode,return_capture=False):
    if mode not in ('default','type0-ignored','all','diffuse','alpha-negative','alpha-high','random','bind','codec-default','codec-values'):raise ValueError('declared small consumer case')
    f=PCWriteBytesFixture();p=f.p;configure_floor(f);animations=f.call(0x454640);controller=controller_input(f);material=f.call(0x41a390);checks=0;states=[];payload=b'';written=b'';maximum=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    f.call(0x423650,this=controller,args=(material,))
    check(p.uint(controller+0x24)==material,'actual binder borrows material on declared input')
    check(p.floats(controller+0x28,16)==tuple(v for offset in (0x88,0x78,0x98,0xa8) for v in p.floats(material+offset,4)),'saved ambient/diffuse/specular/emissive order')
    def colors(owner):return [list(p.floats(owner+offset,4)) for offset in (0x88,0x78,0x98,0xa8)]
    if mode=='bind':
        saved=colors(material);p.put_floats(material+0x78,tuple(float(i) for i in range(17)))
        f.call(0x423650,this=controller,args=(material,));refreshed=colors(material)
        check([list(p.floats(controller+0x28+16*i,4)) for i in range(4)]==refreshed,'alias refreshes saved colors without restore')
        p.put_floats(material+0x78,tuple(float(i+20) for i in range(17)));other=f.call(0x41a390)
        f.call(0x423650,this=controller,args=(other,));check(colors(material)==refreshed,'different bind restores previous four colors')
        states=[saved,refreshed,colors(material),colors(other)];f.call(0x423650,this=controller,args=(0,));check(p.uint(controller+0x24)==0,'explicit null bind restores and detaches')
        f.call(p.uint(p.uint(other)),this=other,args=(1,))
    elif mode.startswith('codec'):
        nested=b''
        for i in range(4):
            nested+=(field(0,struct.pack('<I',0x80402010+i))+field(1,struct.pack('<I',0xff112244+i))+field(2,struct.pack('<I',7))+field(6,struct.pack('<f',i*.25-.5)) if mode=='codec-values' else b'')+b'\0'
        nested+=(field(0,struct.pack('<I',7))+field(4,struct.pack('<f',.25)) if mode=='codec-values' else b'')+b'\0'
        payload=field(0,nested)+b'\0';f.data=payload;f.position=0;serializer=f.call(0x441200)
        result=f.call(0x4412e0,this=serializer+0x10,args=(f.stream,controller))&255
        check(result==1 and f.position==len(payload) and not f.errors,'actual MatColor five-section reader on declared target');maximum=sum(p.visits.values())
        f.data=b'';f.position=0;result=f.call(0x441740,this=serializer+0x10,args=(f.stream,controller))&255
        check(result==1 and not f.errors,'actual nested writer');written=f.data;maximum=max(maximum,sum(p.visits.values()))
        f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    else:
        for i,offset in enumerate(OFFSETS):
            if mode!='default':p.put_uint(controller+offset+0x10,0x80402010+i);p.put_uint(controller+offset+0x14,0xff112244+i);p.put_floats(controller+offset+0x3c,(i*.25-.5,))
            if mode in ('all','random') or (mode=='diffuse' and i==1):p.put_uint(controller+offset+0x4c,6 if mode=='random' else 7)
        if mode in ('all','random','alpha-negative','alpha-high'):
            p.put_uint(controller+0x1dc,6 if mode=='random' else 7);p.put_floats(controller+0x1a8+0x24,(-.5 if mode=='alpha-negative' else 2. if mode=='alpha-high' else .25,))
        if mode=='type0-ignored':p.put_floats(controller+0x1a8+0x24,(.25,))
        for delta in (.25,.5,-.25):
            f.call(0x423190,this=controller,args=(bits(delta),));f.call(0x4373e0,this=controller)
            maximum=max(maximum,sum(p.visits.values()));states.append([delta,colors(material),[p.floats(controller+offset+0x28,1)[0] for offset in OFFSETS]+[p.floats(controller+0x1b8,1)[0]]])
            check(p.floats(controller+0x1c,1)==p.floats(controller+0x20,1),'actual render clock consumed')
    # No attachment to material+74, no fake owner/destructor claim for backing.
    f.call(p.uint(p.uint(material)),this=material,args=(1,));f.call(0x4545d0,this=animations,args=(1,));cleanup(f,())
    check(set(f.allocations)==set(f.freed),'all actual leaves/material/serializer/manager allocations released')
    capture=[mode,payload.hex(),states,written.hex()];print('MATERIAL_COLOR_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original material color consumer {mode}; heap={p.allocated}; maxInstructions={maximum}; factoryExcluded=true',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
