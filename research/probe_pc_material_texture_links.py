#!/usr/bin/env python3
"""Original Material/StdLayer -> runtime DXTexture via the common reference core.

Tiny flat1x1 palette-free texture, explicit valid RTTI input and declared COM.
No cold startup, full-FFPS claim, engine-function seam, GPU or cap retry.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_texture_fixtures import TextureUploadBoundaryFixture
from pc_loader_fixtures import empty_manager,empty_fat
from probe_pc_material_links import material_rtti
from probe_pc_node_serializer import field

def texture_rtti(f):
    material_rtti(f);p=f.p;tree=p.uint(0x755378);root=p.uint(p.uint(tree+0x18)+4)
    # StdLayer234C remains the left node; DXTexture3F36 is a valid larger root.
    p.put_uint(root+12,0x3f3651b6);p.put_uint(root+16,0x763210);p.put_uint(0x763210+0x4c,0x4ab520)
    for record,identity,parent in ((0x763210,0x3f3651b6,0x75df10),(0x75df10,0x2f281e13,0x7603a0),
                                  (0x7603a0,0x46f043fe,0x7555f8)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    for identity in (0x234c576b,0x3f3651b6):
        if f.call(0x4143f0,this=tree,args=(identity,))&255!=1:raise AssertionError('actual RTTI lookup of explicit texture/layer tree')

def main(mode,return_capture=False):
    if mode not in ('inline','repeat','two-layers','null-after','orphan'):raise ValueError('five tiny material/texture reference cases')
    texture_body=struct.pack('<5I',0x3f3651b6,0x4f4f4253,1,1,3)+b'\0'+struct.pack('<I',1)+b'\1\2\3\4'
    reference=struct.pack('<II',7,len(texture_body))+texture_body
    prefix=field(3,struct.pack('<I',2))+field(4,struct.pack('<I',0x234c576b))
    payload=(b'' if mode=='orphan' else prefix)+field(10,reference)
    if mode=='repeat':payload+=field(10,struct.pack('<II',7,0))
    if mode=='two-layers':payload+=field(4,struct.pack('<I',0x234c576b))+field(10,struct.pack('<II',7,0))
    if mode=='null-after':payload+=field(10,struct.pack('<I',0))
    payload+=b'\0';directory=struct.pack('<IIHIII',1,7,0,0x3f3651b6,0,len(texture_body))
    f=TextureUploadBoundaryFixture(directory+payload);p=f.p;texture_rtti(f);f.pitch=8
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x42f690);texture_serializer=f.call(0x4b24c0)
    f.call(0x422d90,this=manager,args=(0x6160348b,serializer,0xff,3))
    f.call(0x422d90,this=manager,args=(0x3f3651b6,texture_serializer,0xff,3))
    checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual texture FAT')
    entry=f.call(0x4664c0,this=fat,args=(7,));material=f.call(0x41a390)
    result=f.call(0x42f670,this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL_TEXTURE_READ',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'errors',f.errors,flush=True)
    check(result==1 and f.position==len(f.data) and not f.errors,'complete original tiny material/texture read')
    texture=p.uint(entry+0x20);state=[]
    if mode=='orphan':check(texture==0 and f.texture_refs==0,'orphan texture field skipped without resolving or creating texture')
    else:
        check(f.allocations.get(texture)==0x4c,'actual runtime DXTexture factory')
        layer_pass=p.uint(material+0x4c);count=p.uint(layer_pass+0x14)
        check(count==(2 if mode=='two-layers' else 1),'expected standard layer count')
        for index in range(count):
            holder=p.uint(p.uint(layer_pass+0x18+4*index)+0x10)
            state.append(p.uint(holder+0x34)==texture)
        print('MATERIAL_TEXTURE_HOLDERS',state,'texture_deleted',texture in f.freed,flush=True)
        if mode=='null-after':
            check(state==[False] and texture in f.freed and p.uint(holder+0x34)==0,'field10 NULL clears/deletes fallback; unlike fields11/12 it calls setter unconditionally')
            check(p.uint(entry+0x20)==texture and f.texture_refs==0,'native FAT retains stale deleted texture address; no re-resolve or native freed-pointer dereference')
        else:
            check(all(state),'repeat is same-pointer no-op; two layers share canonical texture')
            check((p.uint(texture+8)&65535)==count,'one native retained edge per material texture')
            check(bytes(p.mu.mem_read(f.pixel_storage,4))==b'\1\2\3\4','actual runtime pixel payload reached declared surface')
    f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
    check(f.call(0x4672c0,this=serializer,args=(material,))&255==1,'actual material-to-texture recursive index')
    f.data=b'';f.position=0
    result=f.call(p.uint(p.uint(serializer+0x10)),this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL_TEXTURE_WRITE',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),'errors',f.errors,flush=True)
    check(result==1 and not f.errors,'actual native common reference writer on runtime texture')
    capture=[mode,payload.hex(),state,f.data.hex()]
    f.call(0x466760,this=fat);f.call(p.uint(p.uint(material)),this=material,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native material/layer/texture/registry allocations freed')
    check(f.device_refs==1 and f.texture_refs==f.surface_refs==0 and not f.locked,'native teardown releases all acquired COM refs')
    print('MATERIAL_TEXTURE_CAPTURE',capture,flush=True);print(f'PASS {checks}/{checks}: actual material/texture {mode}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
