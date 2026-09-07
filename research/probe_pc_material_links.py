#!/usr/bin/env python3
"""Actual Model->MaterialData->Pass->StdLayer read/index/write, bounded graph.

Explicit valid two-entry RTTI input; not cold startup or disabled logo retry.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_fat,empty_manager
from probe_pc_node_relationships import node_rtti
from probe_pc_node_serializer import field
from probe_pc_material_layers import fixture_data

def material_rtti(f):
    p=f.p;node_rtti(f);tree=p.uint(0x755378);head=p.uint(tree+0x18);root=p.uint(head+4);left=p.allocate(24)
    p.put_uint(root+12,0x6160348b);p.put_uint(root+16,0x75d548);p.put_uint(0x75d548+0x4c,0x41a390)
    p.put_uint(root,left);p.put_uint(head,left);p.put_uint(tree+0x1c,2)
    for off in (0,8):p.put_uint(left+off,head)
    p.put_uint(left+4,root);p.put_uint(left+12,0x234c576b);p.put_uint(left+16,0x75ffa8);p.mu.mem_write(left+20,b'\0\0')
    p.put_uint(0x75ffa8+0x4c,0x460e50)
    for record,identity,parent in ((0x75d548,0x6160348b,0x75dfd0),(0x75dfd0,0x5c0314c5,0x755310),
          (0x75ffa8,0x234c576b,0x75df70),(0x75df70,0x7f577c6d,0x755310),
          (0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030),
          (0x7630e8,0x797b39ec,0x75dfd0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    for identity,record in ((0x6160348b,0x75d548),(0x234c576b,0x75ffa8)):
        if f.call(0x4143f0,this=tree,args=(identity,))&255!=1:raise AssertionError('actual RTTI membership of explicit two-entry tree')

def main(mode,return_capture=False):
    if mode not in {'inline','repeat','clear','prebound'}:raise ValueError('explicit small material graph')
    checks=0
    def check(ok,text):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(text)
    # Explicit color initializes DX power; never compare allocator poison as a default.
    body=struct.pack('<II',0x6160348b,0x4f4f4253)+fixture_data('uv')[:-1]+field(2,struct.pack('<5I',0x12345678,0xabcdef01,0xff102030,0x87654321,0x40600000))+b'\0'
    reference=struct.pack('<II',7,0 if mode=='prebound' else len(body))+(b'' if mode=='prebound' else body)
    payload=field(0,reference)
    if mode=='repeat':payload+=field(0,struct.pack('<II',7,0))
    if mode=='clear':payload+=field(0,struct.pack('<I',0))
    payload+=b'\0\0';directory=struct.pack('<IIHIII',1,7,0,0x6160348b,0,len(body))
    f=PCWriteBytesFixture(directory+payload);p=f.p;material_rtti(f)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x4934c0);material_serializer=f.call(0x42f690);dx_serializer=f.call(0x4b0dd0)
    f.call(0x422d90,this=manager,args=(0x763277db,serializer,0xff,3));f.call(0x422d90,this=manager,args=(0x6160348b,material_serializer,0xff,3))
    # Actual startup 6D4C80 registers this exact runtime class/serializer pair.
    f.call(0x422d90,this=manager,args=(0x797b39ec,dx_serializer,0xff,3))
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual MaterialData FAT load')
    entry=f.call(0x4664c0,this=fat,args=(7,));model=f.call(0x479ed0)
    if mode=='prebound':p.put_uint(entry+0x20,f.call(0x41a390))
    result=f.call(0x4938f0,this=serializer+0x10,args=(f.stream,model))&255
    print('MATERIAL GRAPH READ',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'errors',f.errors,flush=True)
    material=p.uint(entry+0x20);active=p.uint(model+0x20)
    check(result==1 and f.position==len(f.data) and not f.errors,'actual complete Model/Renderable/Material/layer read')
    check(f.allocations.get(material)==0xbc,'actual material factory BC')
    if mode=='clear':
        check(active==0 and material in f.freed and p.uint(entry+0x20)==material,'clear deletes material but FAT retains stale address; no reuse/dereference');state=None
    else:
        check(active==material and p.uint(material+8)&65535==1,'canonical material pointer with one owning Model edge')
        count=p.uint(material+0x48);check(count==(0 if mode=='prebound' else 1),'prebound stays default, inline has one pass')
        state=None
        if count:
            layer_pass=p.uint(material+0x4c);layer=p.uint(layer_pass+0x18);texture=p.uint(layer+0x10)
            state=(bytes(p.mu.mem_read(texture+0x10,36))+bytes(p.mu.mem_read(texture+0x3c,36))+bytes(p.mu.mem_read(texture+0x60,1))).hex()
    f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
    if active:
        record=f.call(p.uint(p.uint(active)+0x10),this=active)
        print('ACTIVE_MATERIAL_RECORD',hex(record),hex(p.uint(record)),'expected record',hex(p.uint(0x75d548)),flush=True)
    def observe_lookup(mu,address,size,user):
        print('MATERIAL_INDEX_LOOKUP','class',hex(p.uint(p.reg('ESP')+4)),'caller',hex(p.uint(p.reg('ESP'))),'manager',hex(p.reg('ECX')),flush=True)
    observer=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_lookup,begin=0x4224f0,end=0x4224f0)
    def observe_object(mu,address,size,user):
        obj=p.uint(p.reg('ESP')+4);print('INDEX_OBJECT',hex(obj),'vtable',hex(p.uint(obj)),'getter',hex(p.uint(p.uint(obj)+0x10)),'caller',hex(p.uint(p.reg('ESP'))),flush=True)
    object_observer=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_object,begin=0x422530,end=0x422530)
    check(f.call(0x4672c0,this=serializer,args=(model,))&255==1,'actual recursive Model/material/pass/layer save indexing')
    p.mu.hook_del(observer)
    p.mu.hook_del(object_observer)
    f.data=b'';f.position=0
    result=f.call(p.uint(p.uint(serializer+0x10)),this=serializer+0x10,args=(f.stream,model))&255
    print('MATERIAL GRAPH WRITE',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),flush=True)
    check(result==1 and not f.errors,'actual common reference writes complete material graph')
    captured=[mode,payload.hex(),state,f.data.hex()]
    f.call(0x466760,this=fat);f.call(0x479f90,this=model,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'whole native small graph allocations freed')
    print('MATERIAL_GRAPH_CAPTURE',captured,flush=True);print(f'PASS {checks}/{checks}: actual Model/material graph {mode}')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
