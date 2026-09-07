#!/usr/bin/env python3
"""Actual Material field6 reader/index/writer with PREBOUND controller input.

Never inline-load/call controller factory41A580 or clone41AF70. Declared external
intrusive pin prevents unproven controller destruction; real materials die.
"""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager,empty_fat
from probe_pc_node_relationships import node_rtti
from probe_pc_node_serializer import field
from probe_pc_material_color import controller_input,OFFSETS
from probe_pc_function_eval import cleanup,configure_floor

def color_rtti(f):
    p=f.p;node_rtti(f);tree=p.uint(0x755378);root=p.uint(p.uint(tree+0x18)+4)
    p.put_uint(root+12,0x4c633e85);p.put_uint(root+16,0x75d788)
    for record,identity,parent in ((0x75d788,0x4c633e85,0x75deb0),(0x75deb0,0x14477ac7,0x75de50),
        (0x75de50,0x4fad24f1,0x760340),(0x760340,0x062c22ed,0x755310)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    def forbidden(_):raise AssertionError('Cold RTTI/factory/clone forbidden for prebound consumer')
    for entry in (0x414c60,0x41a580,0x41af70):p.seams[entry]=forbidden

def main(mode,return_capture=False):
    if mode not in ('prebound','repeat','null-after','shared'):raise ValueError('prebound-only field6 graph')
    payload=field(6,struct.pack('<II',7,0))
    if mode=='repeat':payload+=field(6,struct.pack('<II',7,0))
    if mode=='null-after':payload+=field(6,struct.pack('<I',0))
    payload+=b'\0';directory=struct.pack('<IIHIII',1,7,0,0x4c633e85,0,54)
    f=PCWriteBytesFixture(directory+payload);p=f.p;color_rtti(f);configure_floor(f);animations=f.call(0x454640);controller=controller_input(f);p.mu.mem_write(controller+8,b'\x01\0');checks=0;maximum=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x42f690)
    for identity,ser in ((0x6160348b,serializer),(0x4c633e85,f.call(0x441200))):f.call(0x422d90,this=manager,args=(identity,ser,0xff,3))
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual class-validated FAT directory')
    p.put_uint(f.call(0x4664c0,this=fat,args=(7,))+0x20,controller)
    for i,offset in enumerate(OFFSETS):
        p.put_uint(controller+offset+0x10,0x80402010+i);p.put_uint(controller+offset+0x14,0xff112244+i);p.put_uint(controller+offset+0x4c,7);p.put_floats(controller+offset+0x3c,(i*.25-.5,))
    p.put_uint(controller+0x1dc,7);p.put_floats(controller+0x1cc,(.25,))
    materials=[]
    for i in range(2 if mode=='shared' else 1):
        material=f.call(0x41a390);materials.append(material)
        if i:f.data=payload;f.position=0
        result=f.call(0x42f670,this=serializer+0x10,args=(f.stream,material))&255;maximum=max(maximum,sum(p.visits.values()))
        check(result==1 and f.position==len(f.data) and not f.errors,'actual field6 reader consumes prebound reference')
        check(p.uint(material+0x74)==controller,'same canonical controller retained including repeated/NULL preserve')
    check((p.uint(controller+8)&65535)==1+len(materials) and p.uint(controller+0x24)==materials[-1],'one reference per material plus external pin; last-bound backlink')
    f.call(0x423190,this=controller,args=(0x3f800000,));f.call(0x4373e0,this=controller)
    states=[[[float(v) for v in p.floats(h+offset,4)] for offset in (0x88,0x78,0x98,0xa8)] for h in materials]
    f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
    check(f.call(0x4672c0,this=serializer,args=(materials[0],))&255==1,'actual recursive material/color index')
    f.data=b'';f.position=0;result=f.call(p.uint(p.uint(serializer+0x10)),this=serializer+0x10,args=(f.stream,materials[0]))&255;maximum=max(maximum,sum(p.visits.values()))
    check(result==1 and not f.errors,'actual native material/color/leaf writer');written=f.data
    f.call(0x466760,this=fat)
    for material in materials:
        f.call(0x423a50,this=material,args=(0,));f.call(p.uint(p.uint(material)),this=material,args=(1,))
    check((p.uint(controller+8)&65535)==1,'declared external pin survives: no fake-owner destructor')
    f.call(0x4545d0,this=animations,args=(1,));f.call(0x4228a0,this=manager)
    resources=p.uint(0x75db78)
    if resources:f.call(p.uint(p.uint(resources)),this=resources,args=(1,))
    cleanup(f,());check(set(f.allocations)==set(f.freed),'all actual graph/serializer/manager allocations freed')
    capture=[mode,payload.hex(),states,written.hex()];print('MATERIAL_COLOR_LINK_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: actual prebound color link {mode}; maxInstructions={maximum}; heap={p.allocated}; factoryExcluded=true',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
