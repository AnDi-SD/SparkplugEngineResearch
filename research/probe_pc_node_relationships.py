#!/usr/bin/env python3
"""Original Node reader through actual inline resolver and owning child attach."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture,empty_manager,empty_fat
from probe_pc_node_serializer import field,check
import probe_pc_node_serializer as counters

CLASS=0x695c0f65

def node_rtti(f):
    # Explicit one-entry startup input, not native global registration proof.
    p=f.p;rtti=p.allocate(0x20);head=p.allocate(24);entry_node=p.allocate(24)
    for offset in (0,4,8):p.put_uint(head+offset,entry_node);p.put_uint(entry_node+offset,head)
    p.mu.mem_write(head+20,b'\x01\x01');p.mu.mem_write(entry_node+20,b'\x01\x00')
    p.put_uint(entry_node+12,CLASS);p.put_uint(entry_node+16,0x75dd88)
    p.put_uint(rtti+0x18,head);p.put_uint(rtti+0x1c,1);p.put_uint(0x755378,rtti)
    p.put_uint(0x75dd88+0x4c,0x421e20)
    for record,identity,parent in ((0x75dd88,CLASS,0x7555f8),(0x7555f8,0x44de07fd,0x755310),(0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)

def main(mode):
    if mode not in {'child','repeated-child','prebound-child','null-collision-stop'}:
        raise ValueError('explicit bounded child case required')
    child_fields=field(0,struct.pack('<3f',1,2,3))+b'\0'
    child_body=struct.pack('<II',CLASS,0x4f4f4253)+child_fields
    reference=struct.pack('<II',7,len(child_body))+child_body
    if mode=='prebound-child':reference=struct.pack('<II',7,0)
    payload=field(0,struct.pack('<3f',10,0,0))+field(5,reference)
    if mode=='repeated-child':payload+=field(5,struct.pack('<II',7,0))
    if mode=='null-collision-stop':payload=field(7,struct.pack('<I',0))
    payload+=b'\0'
    directory=struct.pack('<IIHIII',1,7,0,CLASS,0,len(child_body))
    f=PCFileBytesFixture(directory+payload);p=f.p;f.call(0x6d38e0)
    node_rtti(f)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x4638f0);f.call(0x422d90,this=manager,args=(CLASS,serializer,0xff,3))
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual FAT validates explicit Node class')
    fat_entry=f.call(0x4664c0,this=fat,args=(7,));parent=f.call(0x421e20)
    if mode=='prebound-child':
        child=f.call(0x421e20);p.put_floats(child+0x20,(1.,2.,3.));p.put_uint(child+0xb0,p.uint(child+0xb0)|1)
        p.put_uint(fat_entry+0x20,child)
    if mode=='null-collision-stop':
        # Static421ED0 immediately calls464EE0 with the reference as this.
        # Observe the unchecked NULL edge, do NOT execute the dereference.
        p.run(0x463a70,this=serializer+0x10,args=(f.stream,parent),stop_at=0x421ed0)
        check(p.reg('ECX')==parent and p.uint(p.reg('ESP')+4)==0,'null collision forwarded unchecked into attach')
        block_head=p.uint(p.reg('ESP')+0x4c)
        check(f.allocations.get(block_head)==24 and p.uint(block_head)==block_head
              and p.uint(block_head+4)==block_head,'stopped reader owns an empty24-byte block-list sentinel')
        p.put_uint(f.teb,0xffffffff) # explicit frame abort, never resumed
        p.run(0x412420,args=(block_head,),callee_pop=False)
    else:
        result=f.call(0x463a70,this=serializer+0x10,args=(f.stream,parent))&255
        child=p.uint(fat_entry+0x20)
        print('NODE CHILD',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,
              'child',hex(child),'count',p.uint(parent+0x1c),'errors',f.errors,flush=True)
        check(result==1 and not f.errors and f.position==len(f.data),'whole original parent reader consumes child references')
        check(child in f.allocations and p.uint(child)==0x6dc4f4,'actual/prebound Node child, not mocked relationship')
        check(p.uint(parent+0x1c)==1 and p.uint(child+0x2c)==parent,'attach/repeated attach leaves one reciprocal relationship')
        check(p.uint(child+8)&0xffff==1,'parent owns exactly one intrusive child reference')
        check(p.floats(child+0x74,3)==(11.,2.,3.),'final parent world update propagates into actual child')
        check(0x4678b0 in p.visits and 0x421a60 in p.visits,'common reference resolver and native attach actually visited')
    f.call(0x466760,this=fat);f.call(0x422220,this=parent,args=(1,))
    if mode!='null-collision-stop':check(child in f.freed,'parent teardown releases and deletes owned child')
    f.call(0x4228a0,this=manager)
    # Native attach may lazily construct the separately proven SceneManager
    # while searching for the parent's scene. It is not owned by the Node.
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    remaining=[(hex(a),s,hex(p.uint(a))) for a,s in f.allocations.items() if a not in f.freed]
    if remaining:print('REMAINING',remaining,flush=True)
    check(set(f.allocations)==set(f.freed),'all original or explicitly aborted fixture allocations cleaned')
    print(f'PASS {counters.checks}/{counters.checks}: original Node relationship {mode}')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
