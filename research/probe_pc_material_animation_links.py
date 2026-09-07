#!/usr/bin/env python3
"""PC material -> inline AnimTex -> shared runtime DXTexture, bounded graph.

Extends CP19's verified explicit RTTI/COM inputs, not a cold startup or capped
FAT retry. Cold414C60 is forbidden before entry. One tiny COM texture only.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_texture_fixtures import TextureUploadBoundaryFixture
from pc_loader_fixtures import empty_manager,empty_fat
from probe_pc_material_texture_links import texture_rtti
from probe_pc_node_serializer import field

def controller_rtti(f):
    texture_rtti(f);p=f.p;tree=p.uint(0x755378);head=p.uint(tree+0x18);root=p.uint(head+4);left=p.uint(root);right=p.allocate(24)
    p.put_uint(root+12,0x234c576b);p.put_uint(root+16,0x75ffa8);p.put_uint(root+8,right)
    p.put_uint(left+12,0x16fb0e47);p.put_uint(left+16,0x75d1e8)
    p.put_uint(right,head);p.put_uint(right+4,root);p.put_uint(right+8,head)
    p.put_uint(right+12,0x3f3651b6);p.put_uint(right+16,0x763210);p.mu.mem_write(right+20,b'\0\0')
    p.put_uint(head+8,right);p.put_uint(tree+0x1c,3)
    p.put_uint(0x75d1e8+0x4c,0x41a030)
    for record,identity,parent in ((0x75d1e8,0x16fb0e47,0x75deb0),(0x75deb0,0x14477ac7,0x75de50),
                                  (0x75de50,0x4fad24f1,0x760340),(0x760340,0x062c22ed,0x755310)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    for identity in (0x16fb0e47,0x234c576b,0x3f3651b6):
        if f.call(0x4143f0,this=tree,args=(identity,))&255!=1:raise AssertionError('actual membership on declared balanced three-node RTTI input')
    def deny_cold(_):raise AssertionError('Previously capped cold RTTI disabled before entry')
    p.seams[0x414c60]=deny_cold

def main(mode,return_capture=False):
    if mode not in ('inline','repeat','null-after','two-layers'):raise ValueError('four bounded controller graphs')
    texture=struct.pack('<5I',0x3f3651b6,0x4f4f4253,1,1,3)+b'\0'+struct.pack('<I',1)+b'\1\2\3\4'
    frames=struct.pack('<I3fII',3,1.,2.,3.,9,len(texture))+texture+struct.pack('<IIII',9,0,9,0)
    controller=struct.pack('<II',0x16fb0e47,0x4f4f4253)+field(0,frames)+b'\0'
    payload=field(3,struct.pack('<I',2))+field(4,struct.pack('<I',0x234c576b))+field(11,struct.pack('<II',7,len(controller))+controller)
    if mode=='repeat':payload+=field(11,struct.pack('<II',7,0))
    if mode=='null-after':payload+=field(11,struct.pack('<I',0))
    if mode=='two-layers':payload+=field(4,struct.pack('<I',0x234c576b))+field(11,struct.pack('<II',7,0))
    payload+=b'\0'
    directory=struct.pack('<I',2)+struct.pack('<IHIII',7,0,0x16fb0e47,0,len(controller))+struct.pack('<IHIII',9,0,0x3f3651b6,0,len(texture))
    f=TextureUploadBoundaryFixture(directory+payload);p=f.p;controller_rtti(f);f.pitch=8;checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    animation_manager=f.call(0x454640);fat=empty_fat(f);manager,_=empty_manager(f)
    p.put_uint(manager+0x28,fat);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x42f690)
    for identity,ser in ((0x6160348b,serializer),(0x16fb0e47,f.call(0x43c0c0)),(0x3f3651b6,f.call(0x4b24c0))):
        f.call(0x422d90,this=manager,args=(identity,ser,0xff,3))
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual valid registered controller/texture directory')
    material=f.call(0x41a390);result=f.call(0x42f670,this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL_ANIMATION_READ',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'position',f.position,'errors',f.errors,flush=True)
    check(result==1 and f.position==len(f.data) and not f.errors,'complete material/controller/texture graph')
    controller=p.uint(f.call(0x4664c0,this=fat,args=(7,))+0x20);texture=p.uint(f.call(0x4664c0,this=fat,args=(9,))+0x20)
    check(f.allocations.get(controller)==0x4c and f.allocations.get(texture)==0x4c,'actual AnimTex and runtime DXTexture factories')
    count=2 if mode=='two-layers' else 1;layer_pass=p.uint(material+0x4c)
    holders=[p.uint(p.uint(layer_pass+0x18+4*i)+0x10) for i in range(count)]
    check(all(p.uint(h+0x64)==controller for h in holders) and (p.uint(controller+8)&65535)==count,'canonical controller retained once per layer including NULL-preserve')
    check(p.uint(controller+0x24)==holders[-1],'last bound layer owns backlink')
    check((p.uint(texture+8)&65535)==3,'each duplicate frame reference retains texture')
    f.call(0x423190,this=controller,args=(0x3f800000,));f.call(0x467b70,this=holders[0],args=(0,))
    active=[p.uint(h+0x34)==texture for h in holders]
    check(active==([True] if count==1 else [False,True]),'rendering first holder updates controller last-bound holder, not necessarily caller')
    check((p.uint(texture+8)&65535)==4,'three frame slots plus active material fallback')
    f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
    check(f.call(0x4672c0,this=serializer,args=(material,))&255==1,'recursive material/controller/texture index')
    f.data=b'';f.position=0;result=f.call(p.uint(p.uint(serializer+0x10)),this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL_ANIMATION_WRITE',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),flush=True)
    check(result==1 and not f.errors,'actual graph writer shares one texture across fallback and frame slots')
    capture=[mode,payload.hex(),active,f.data.hex()]
    f.call(0x466760,this=fat);f.call(p.uint(p.uint(material)),this=material,args=(1,))
    check(p.uint(animation_manager+0x24)==p.uint(animation_manager+0x28)==0,'graph destruction unregisters controller')
    f.call(0x4545d0,this=animation_manager,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        owner=p.uint(address)
        if owner:f.call(p.uint(p.uint(owner)),this=owner,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native graph and manager allocations freed')
    check(f.device_refs==1 and f.texture_refs==f.surface_refs==0 and not f.locked,'tiny COM baseline fully restored')
    print('MATERIAL_ANIMATION_CAPTURE',capture,flush=True);print(f'PASS {checks}/{checks}: original material animation {mode}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
