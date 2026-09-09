#!/usr/bin/env python3
"""Actual RenderNode -> Model reference, owning append and secondary writers.

The inline model has no mesh/material/fog; this proves that precise graph edge,
not mesh/material construction. Startup/renderer storage remain explicit.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_fat,empty_manager
from probe_pc_node_relationships import node_rtti
from probe_pc_node_serializer import field

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

def main(mode,return_capture=False,target='render-node'):
    if target not in {'render-node','skybox'}:raise ValueError('explicit actual render support class')
    if mode=='null':raise ValueError('Disabled whole NULL diagnostic: formatting IAT4169DF->0033D03C unmapped; use distinct stop-before diagnostic boundary, never forward host API')
    if mode not in {'inline','repeat','prebound','null-stop'}:raise ValueError('explicit bounded RenderNode relation case')
    body=struct.pack('<II',0x763277db,0x4f4f4253)+field(2,struct.pack('<I',0))+field(3,struct.pack('<I',17))+b'\0'+field(1,struct.pack('<I',9))+b'\0'
    reference=struct.pack('<II',7,len(body))+body
    if mode=='prebound':reference=struct.pack('<II',7,0)
    if mode=='null-stop':reference=struct.pack('<I',0)
    payload=b'\0'+field(0,reference)
    if mode=='repeat':payload+=field(0,struct.pack('<II',7,0))
    payload+=b'\0'
    directory=struct.pack('<IIHIII',1,7,0,0x763277db,0,len(body))
    f=PCWriteBytesFixture(directory+payload);p=f.p;f.call(0x6d38e0);node_rtti(f)
    tree=p.uint(0x755378);entry_node=p.uint(p.uint(tree+0x18)+4)
    p.put_uint(entry_node+12,0x763277db);p.put_uint(entry_node+16,0x760cf8);p.put_uint(0x760cf8+0x4c,0x479ed0)
    for record,identity,parent in ((0x75e150,0x603625d0,0x75dd88),(0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    p.put_uint(0x762e10,0x7a7124af);p.put_uint(0x762e10+0x48,0x75e150)
    p.put_uint(0x75db68,p.allocate(0xc9c8))
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x469040);model_serializer=f.call(0x4934c0)
    f.call(0x422d90,this=manager,args=(0x603625d0,serializer,0xff,3));f.call(0x422d90,this=manager,args=(0x763277db,model_serializer,0xff,3))
    if target=='skybox':f.call(0x6d4b00) # actual registration creates the same RenderNode serializer
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual FAT model membership')
    entry=f.call(0x4664c0,this=fat,args=(7,));node=f.call(0x49e4c0 if target=='skybox' else 0x425520)
    if target=='skybox':check(f.allocations[node]==0x1d4 and p.uint(node)==0x6eecec,'actual SkyBox factory/type/extent')
    if mode=='prebound':p.put_uint(entry+0x20,f.call(0x479ed0))
    if mode=='null-stop':
        p.run(0x469190,this=serializer+0x10,args=(f.stream,node),stop_at=0x469288)
        check(p.reg('EAX')==0 and p.uint(node+0xbc)==0,'actual NULL resolver branches to diagnostic before any append')
        head=p.uint(p.reg('ESP')+0x10)
        check(f.allocations.get(head)==24 and p.uint(head)==head,'stopped derived reader empty block sentinel')
        p.put_uint(f.teb,0xffffffff);p.run(0x412420,args=(head,),callee_pop=False)
        f.call(0x466760,this=fat);f.call(0x4255d0,this=node,args=(1,));f.call(0x4228a0,this=manager)
        for address in (0x75db90,0x75db78,0x75526c,0x755264):
            owned=p.uint(address)
            if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
        check(set(f.allocations)==set(f.freed),'all native or explicitly aborted storage freed')
        print(f'PASS {checks}/{checks}: NULL relation stop before diagnostics; complete return not claimed')
        return [mode,payload.hex(),None,None] if return_capture else 0
    result=f.call(0x469190,this=serializer+0x10,args=(f.stream,node))&255
    print('RN RELATION',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'errors',f.errors,flush=True)
    check(result==int(mode!='null'),'actual reader return incl NULL rejection')
    if mode!='null':
        model=p.uint(entry+0x20);count=(p.uint(node+0xc0)-p.uint(node+0xbc))//4
        check(model in f.allocations and f.allocations[model]==0x60,'actual newly created or explicitly prebound Model')
        check(count==(2 if mode=='repeat' else 1) and all(p.uint(p.uint(node+0xbc)+4*i)==model for i in range(count)),'renderable aliases retained once per occurrence, unlike child no-op')
        check(p.uint(model+8)&65535==count,'owning append retains each relation')
        check(f.position==len(f.data) and not f.errors,'complete original derived/base sections and references consumed')
        state=[bytes(p.mu.mem_read(model+0x18,1))[0],p.uint(model+0x1c),p.uint(model+0x5c),count,bytes(p.mu.mem_read(node+0xc8,32)).hex()]
        check(state[:3]==([1,0,3] if mode=='prebound' else [0,17,9]),'actual Model/Renderable readers applied ordered inherited sections')
        # Read publication sets entry.object, not the save-by-object map.
        # A real save requires the separate native recursive index pass.
        # The first scout omitted it and faulted at4673AC on NULL FAT lookup;
        # that was not a cap or a successful save. Recreate save state normally.
        f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
        check(f.call(0x4672c0,this=serializer,args=(node,))&255==1,'actual recursive save index pass, including Model dependencies')
        f.data=b'';f.position=0
        written=f.call(0x469340,this=serializer+0x10,args=(f.stream,node))&255
        print('RN WRITE',mode,'result',written,'instructions',sum(p.visits.values()),'hex',f.data.hex(),'errors',f.errors,flush=True)
        check(written==1 and not f.errors,'actual RenderNode writer resolves Model secondary writer')
        captured=[mode,payload.hex(),state,f.data.hex()]
    else:
        check(p.uint(node+0xbc)==0,'NULL relation never appended')
        captured=[mode,payload.hex(),None,None]
    f.call(0x466760,this=fat);f.call(0x49e590 if target=='skybox' else 0x4255d0,this=node,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native graph/serializer/storage allocations released')
    print('RN_CAPTURE',captured,flush=True);print(f'PASS {checks}/{checks}: actual {target}/Model {mode}')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2],target=sys.argv[3] if len(sys.argv)>3 else 'render-node'))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
