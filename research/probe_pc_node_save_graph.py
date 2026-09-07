#!/usr/bin/env python3
"""Native Node recursive indexing and inline write-reference, not whole Save."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager,empty_fat
from probe_pc_node_relationships import node_rtti,CLASS
from probe_pc_node_serializer import check
import probe_pc_node_serializer as counters

def main(return_capture=False):
    f=PCWriteBytesFixture();p=f.p;node_rtti(f);f.call(0x6d38e0)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,2);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x4638f0);f.call(0x422d90,this=manager,args=(CLASS,serializer,0xff,3))
    root=f.call(0x421e20);child=f.call(0x421e20);p.put_floats(child+0x20,(1.,2.,3.))
    f.call(0x421a60,this=root,args=(child,))
    indexed=f.call(0x4672c0,this=serializer,args=(root,))&255
    print('NODE INDEX',indexed,'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    check(indexed==1 and 0x4639d0 in p.visits and 0x467300 in p.visits,'actual Node relationship finalizer recursively indexes child')
    parent_entry=f.call(0x4664f0,this=fat,args=(root,));child_entry=f.call(0x4664f0,this=fat,args=(child,))
    check(p.uint(parent_entry+4)==1 and p.uint(child_entry+4)==2 and p.uint(fat+0x10)==3,'DFS root/child IDs without duplicate entries')
    check(f.call(0x4672c0,this=serializer,args=(root,))&255==1 and 0x4639d0 not in p.visits,'second indexing skips already indexed graph')
    result=f.call(0x467350,this=serializer,args=(f.stream,root))&255
    print('NODE SAVE GRAPH',result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),'errors',f.errors,flush=True)
    check(result==1 and not f.errors and f.position==len(f.data),'original recursive reference write succeeds')
    check(0x463f10 in p.visits and 0x472d30 in p.visits and 0x472e20 in p.visits,'actual Node writer and nested block patching executed')
    check(p.uint(parent_entry+0x14)==8 and p.uint(parent_entry+0x18)==len(f.data)-8,'root index extent covers own header and inline child')
    child_offset=p.uint(child_entry+0x14);child_size=p.uint(child_entry+0x18)
    check(f.data[child_offset:child_offset+8]==struct.pack('<II',CLASS,0x4f4f4253) and child_size==25,'child extent starts at SBOO header with exact25 bytes')
    original=f.data
    check(f.call(0x467350,this=serializer,args=(f.stream,root))&255==1 and f.data==original+struct.pack('<II',1,0),'repeated root reference does not re-emit any child')
    f.call(0x466760,this=fat);f.call(0x422220,this=root,args=(1,));check(child in f.freed,'parent owns and deletes child after writer')
    f.call(0x4228a0,this=manager)
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all original graph/index/writer allocations released')
    print(f'PASS {counters.checks}/{counters.checks}: original PC Node graph indexing/write reference')
    return original.hex() if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
