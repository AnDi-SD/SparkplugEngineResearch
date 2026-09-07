#!/usr/bin/env python3
"""Tiny original standard-layer grammar with explicit valid one-entry PC RTTI.

Not a retry/resumption of the disabled whole real logo MaterialData scout.
No layer factory, reference resolver or codec is replaced by a seam.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_relationships import node_rtti
from probe_pc_node_serializer import field

def fixture_data(mode):
    data=field(3,struct.pack('<I',2))+field(4,struct.pack('<I',0x234c576b))
    if mode in {'states','uv','uv-zero','uv-repeat','repeat'}:data+=field(17,struct.pack('<9I',9,8,7,6,5,4,3,2,1))
    if mode=='repeat':data+=field(17,struct.pack('<9I',0,1,2,3,4,5,6,7,8))
    if mode in {'uv','uv-zero','uv-repeat'}:data+=field(9,struct.pack('<I9f',0 if mode=='uv-zero' else 2,1,2,3,4,5,6,7,8,9))
    if mode=='uv-repeat':data+=field(9,struct.pack('<I9f',0,9,8,7,6,5,4,3,2,1))
    if mode=='legacy':data+=field(8,struct.pack('<9I',9,8,7,6,5,4,3,2,1))
    return data+b'\0'

def main(mode,return_capture=False):
    if mode not in {'default','states','uv','uv-zero','uv-repeat','repeat','legacy'}:raise ValueError('explicit tiny standard layer mode')
    checks=0
    def check(ok,text):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(text)
    data=fixture_data(mode);f=PCWriteBytesFixture(data);p=f.p;node_rtti(f)
    manager=p.uint(0x755378);node=p.uint(p.uint(manager+0x18)+4)
    p.put_uint(node+12,0x234c576b);p.put_uint(node+16,0x75ffa8);p.put_uint(0x75ffa8+0x4c,0x460e50)
    for record,identity,parent in ((0x75ffa8,0x234c576b,0x75df70),(0x75df70,0x7f577c6d,0x755310)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    material=f.call(0x41a390);serializer=f.call(0x42f690);secondary=p.uint(serializer+0x10)
    result=f.call(p.uint(secondary+8),this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL LAYER READ',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'errors',f.errors,flush=True)
    check(result==1 and f.position==len(data) and not f.errors,'actual full tiny material-layer read')
    check(p.uint(material+0x48)==1,'one actual owning pass')
    layer_pass=p.uint(material+0x4c);check(f.allocations[layer_pass]==0x38 and p.uint(layer_pass+0x14)==1,'actual pass38 with one layer')
    layer=p.uint(layer_pass+0x18);texture=p.uint(layer+0x10)
    check(f.allocations[layer]==0x14 and f.allocations[texture]==0x68,'actual Std14/materialtexture68 factories, no fake payload')
    refs=[p.uint(obj+8)&65535 for obj in (layer_pass,layer,texture)]
    print('LAYER_REFS',refs,flush=True)
    check(refs==[1,0,0],'material->pass intrusive; pass->layer and layer->texture direct-delete owners, not retained references')
    state=(bytes(p.mu.mem_read(texture+0x10,36))+bytes(p.mu.mem_read(texture+0x3c,36))+bytes(p.mu.mem_read(texture+0x60,1))).hex()
    print('LAYER_STATE',state,flush=True)
    check(f.call(p.uint(secondary+4),this=serializer+0x10,args=(material,))&255==1,'actual material/layer recursive index with NULL external references')
    f.data=b'';f.position=0
    result=f.call(p.uint(secondary),this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL LAYER WRITE',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),'errors',f.errors,flush=True)
    check(result==1 and not f.errors,'actual common material/layer writer')
    captured=[mode,data.hex(),state,f.data.hex()]
    f.call(p.uint(p.uint(material)),this=material,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    for address in (0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native material/pass/layer/texture objects released')
    print('MATERIAL_LAYER_CAPTURE',captured,flush=True);print(f'PASS {checks}/{checks}: original tiny standard-layer {mode}')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
