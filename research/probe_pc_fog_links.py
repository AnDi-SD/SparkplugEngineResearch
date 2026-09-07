#!/usr/bin/env python3
"""Actual Model/Renderable -> nonempty Fog ownership/read/index/write chain."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_fat,empty_manager
from probe_pc_node_relationships import node_rtti
from probe_pc_node_serializer import field
from probe_pc_fog_serializer import fixture_data

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

def main(mode,return_capture=False):
    if mode not in {'inline','repeat','clear','prebound','named'}:raise ValueError('explicit bounded Fog owner case')
    body=struct.pack('<II',0x7ac95aec,0x4f4f4253)+fixture_data('values')
    reference=struct.pack('<II',7,len(body))+body
    if mode=='prebound':reference=struct.pack('<II',7,0)
    payload=field(1,reference)
    if mode=='repeat':payload+=field(1,struct.pack('<II',7,0))
    if mode=='clear':payload+=field(1,struct.pack('<I',0))
    payload+=b'\0\0'
    name=b'fog-prefix' if mode=='named' else b''
    directory=struct.pack('<IIH',1,7,len(name))+name+struct.pack('<III',0x7ac95aec,0,len(body))
    f=PCWriteBytesFixture(directory+payload);p=f.p;node_rtti(f)
    tree=p.uint(0x755378);entry_node=p.uint(p.uint(tree+0x18)+4)
    p.put_uint(entry_node+12,0x7ac95aec);p.put_uint(entry_node+16,0x75cf48);p.put_uint(0x75cf48+0x4c,0x419e90)
    for record,identity,parent in ((0x75cf48,0x7ac95aec,0x755310),(0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x4934c0);fog_serializer=f.call(0x43b830)
    f.call(0x422d90,this=manager,args=(0x763277db,serializer,0xff,3));f.call(0x422d90,this=manager,args=(0x7ac95aec,fog_serializer,0xff,3))
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual FAT Fog membership')
    entry=f.call(0x4664c0,this=fat,args=(7,));model=f.call(0x479ed0)
    if mode=='prebound':p.put_uint(entry+0x20,f.call(0x419e90))
    result=f.call(0x4938f0,this=serializer+0x10,args=(f.stream,model))&255
    print('FOG LINK',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'errors',f.errors,flush=True)
    fog=p.uint(entry+0x20);active=p.uint(model+0x24)
    check(result==1 and f.position==len(f.data) and not f.errors,'complete actual Model/Renderable/Fog readers')
    check(fog in f.allocations and f.allocations[fog]==0x28,'native created/prebound Fog28')
    if mode=='clear':
        check(active==0 and fog in f.freed,'NULL replacement releases and deletes sole-owned Fog')
        check(p.uint(entry+0x20)==fog,'native FAT retains stale pointer after owning edge clears; no re-use executed')
        state=None
    else:
        check(active==fog and p.uint(fog+8)&65535==1,'same Fog reference remains one owned edge despite repeats')
        state=bytes(p.mu.mem_read(fog+0x14,20)).hex()
        if mode=='named':
            named=p.uint(fog+0x10)
            check(named==0 and 0x4671f0 in p.visits and 0x4130f0 not in p.visits,'actual common name helper skips Fog due engine RTTI, despite physical NamedObject prefix')
    f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
    check(f.call(0x4672c0,this=serializer,args=(model,))&255==1,'actual Model/Renderable/Fog recursive save index')
    f.data=b'';f.position=0
    written=f.call(p.uint(p.uint(serializer+0x10)),this=serializer+0x10,args=(f.stream,model))&255
    print('FOG LINK WRITE',mode,'result',written,'instructions',sum(p.visits.values()),'hex',f.data.hex(),flush=True)
    check(written==1 and not f.errors,'actual complete Model writer includes Fog secondary codec')
    captured=[mode,payload.hex(),state,f.data.hex()]
    f.call(0x466760,this=fat);f.call(0x479f90,this=model,args=(1,));f.call(0x4228a0,this=manager)
    check(fog in f.freed,'Fog eventually released exactly once by native owner lifecycle')
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native graph allocations released')
    print('FOG_LINK_CAPTURE',captured,flush=True);print(f'PASS {checks}/{checks}: nonempty Model/Fog {mode}')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
