#!/usr/bin/env python3
"""PC material -> inline/shared UV -> nested Trans/Function, no renderer seam.

Valid explicit two-node RTTI input; previously capped cold registration is
forbidden before entry. Runtime calls actual UV directly, so COM is not needed.
"""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager,empty_fat
from probe_pc_node_relationships import node_rtti
from probe_pc_node_serializer import field
from probe_pc_function_eval import cleanup,configure_floor


def uv_rtti(f):
    p=f.p;node_rtti(f);tree=p.uint(0x755378);head=p.uint(tree+0x18);root=p.uint(head+4);left=p.allocate(24)
    p.put_uint(root+12,0x234c576b);p.put_uint(root+16,0x75ffa8);p.put_uint(root,left)
    p.put_uint(head,left);p.put_uint(tree+0x1c,2)
    for off,value in ((0,head),(4,root),(8,head),(12,0x1c0053d6),(16,0x75d3c8)):p.put_uint(left+off,value)
    p.mu.mem_write(left+20,b'\0\0')
    for record,identity,parent,factory in ((0x75ffa8,0x234c576b,0x75df70,0x460e50),
          (0x75df70,0x7f577c6d,0x755310,0),(0x75d3c8,0x1c0053d6,0x75deb0,0x41a210),
          (0x75deb0,0x14477ac7,0x75de50,0),(0x75de50,0x4fad24f1,0x760340,0),
          (0x760340,0x062c22ed,0x755310,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
        if factory:p.put_uint(record+0x4c,factory)
    for identity in (0x1c0053d6,0x234c576b):
        if f.call(0x4143f0,this=tree,args=(identity,))&255!=1:raise AssertionError('actual RTTI membership')
    def deny_cold(_):raise AssertionError('Previously capped cold RTTI disabled before entry')
    p.seams[0x414c60]=deny_cold


def main(mode,return_capture=False):
    if mode not in ('inline','repeat','null-after','two-layers'):raise ValueError('four tiny UV graphs')
    functions=b''.join((field(4,struct.pack('<f',value)) if value else b'')+b'\0' for value in (2.,3.,0.,1.,1.,1.,.25))
    trans=field(0,functions+struct.pack('<6f',.5,1.,2.,0.,0.,1.))+b'\0'
    controller=struct.pack('<II',0x1c0053d6,0x4f4f4253)+field(0,trans)+b'\0'
    payload=field(3,struct.pack('<I',2))+field(4,struct.pack('<I',0x234c576b))
    payload+=field(9,struct.pack('<I9f',1,2.,.5,0.,.25,3.,0.,.5,.75,1.))
    payload+=field(12,struct.pack('<II',7,len(controller))+controller)
    if mode in ('repeat','two-layers'):
        if mode=='two-layers':payload+=field(4,struct.pack('<I',0x234c576b))
        payload+=field(12,struct.pack('<II',7,0))
    if mode=='null-after':payload+=field(12,struct.pack('<I',0))
    payload+=b'\0'
    directory=struct.pack('<IIHIII',1,7,0,0x1c0053d6,0,len(controller))
    f=PCWriteBytesFixture(directory+payload);p=f.p;uv_rtti(f);configure_floor(f);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    animations=f.call(0x454640);fat=empty_fat(f);manager,_=empty_manager(f)
    p.put_uint(manager+0x28,fat);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x42f690)
    for identity,ser in ((0x6160348b,serializer),(0x1c0053d6,f.call(0x440b00))):f.call(0x422d90,this=manager,args=(identity,ser,0xff,3))
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual registered UV directory load')
    material=f.call(0x41a390);result=f.call(0x42f670,this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL_UV_READ',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'position',f.position,'errors',f.errors,flush=True)
    check(result==1 and f.position==len(f.data) and not f.errors,'actual complete material/UV nested graph')
    controller=p.uint(f.call(0x4664c0,this=fat,args=(7,))+0x20)
    check(f.allocations.get(controller)==0x1fc,'actual UV factory')
    count=2 if mode=='two-layers' else 1;layer_pass=p.uint(material+0x4c)
    holders=[p.uint(p.uint(layer_pass+0x18+4*i)+0x10) for i in range(count)]
    check(all(p.uint(h+0x38)==controller for h in holders) and (p.uint(controller+8)&65535)==count,'one UV identity retained per holder, including NULL preserve')
    check(p.uint(controller+0x24)==holders[-1],'last bound holder backlink')
    check(all(p.uint(h+0x30)==2 for h in holders),'bind writes texture coordinate mode2')
    f.call(0x423190,this=controller,args=(0x3f800000,));f.call(0x434820,this=controller)
    states=[list(p.floats(h+0x3c,9)) for h in holders]
    check(p.floats(controller+0x1c,2)==(1.,1.),'actual UV consumes accumulated time')
    f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
    check(f.call(0x4672c0,this=serializer,args=(material,))&255==1,'recursive material/UV indexing')
    f.data=b'';f.position=0;result=f.call(p.uint(p.uint(serializer+0x10)),this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL_UV_WRITE',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),flush=True)
    check(result==1 and not f.errors,'shared UV writer through common reference core')
    capture=[mode,payload.hex(),states,f.data.hex()]
    f.call(0x466760,this=fat);f.call(p.uint(p.uint(material)),this=material,args=(1,))
    check(controller in f.freed and p.uint(animations+0x24)==p.uint(animations+0x28)==0,'graph teardown destroys/unregisters UV exactly once')
    f.call(0x4545d0,this=animations,args=(1,));f.call(0x4228a0,this=manager)
    resources=p.uint(0x75db78)
    if resources:f.call(p.uint(p.uint(resources)),this=resources,args=(1,))
    cleanup(f,())
    check(set(f.allocations)==set(f.freed),'all actual tracked graph/manager allocations freed')
    print('MATERIAL_UV_CAPTURE',json.dumps(capture),flush=True);print(f'PASS {checks}/{checks}: original material UV {mode}',flush=True)
    return capture if return_capture else 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
