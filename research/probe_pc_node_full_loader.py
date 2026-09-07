#!/usr/bin/env python3
"""Actual PC whole FFPS load of native-written Node graph, two live roots."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture,empty_manager,empty_fat
from probe_pc_node_relationships import node_rtti,CLASS
from probe_pc_node_save_graph import main as native_writer
from probe_pc_node_serializer import check
import probe_pc_node_serializer as counters

def main():
    raise ValueError('Whole constructed Node graph first-load capped at100k/88FDE3 on2026-09-06; disabled, never retry/resume. Use independently bounded Node field/reference/index/writer evidence. Whole path remains open.')
    # Archived single scout specification below. Not a runnable success profile.
    # Separate complete native writer call; no resumed frame or generated source
    # bytes. Only the outer FFPS/FAT envelope is a declared test construction.
    reference=bytes.fromhex(native_writer(True));body=reference[8:]
    origin=72;raw=struct.pack('<7I',0x53504646,0x26,0,origin+len(body),2,origin,len(body))
    raw+=struct.pack('<I',2)+struct.pack('<IHIII',1,0,CLASS,0,49)+struct.pack('<IHIII',2,0,CLASS,23,25)+struct.pack('<I',0)+body
    f=PCFileBytesFixture(raw);p=f.p;node_rtti(f);f.call(0x6d38e0)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x4638f0);f.call(0x422d90,this=manager,args=(CLASS,serializer,0xff,3))
    roots=[];children=[]
    for ordinal in range(2):
        f.position=0;p.put_uint(f.stream+0x14,0)
        root=f.call(0x422b50,this=manager,args=(f.stream,));visited=set(p.visits)
        print('WHOLE NODE FFPS',ordinal,'root',hex(root),'instructions',sum(p.visits.values()),'heap',p.allocated,'errors',f.errors,flush=True)
        check(root in f.allocations and p.uint(root)==0x6dc4f4,'actual whole loader returns Node root')
        check(all(a in visited for a in (0x422260,0x466b90,0x4aab80,0x422940,0x467550,0x463a70,0x4678b0,0x421a60,0x466760)),'original envelope/factory/Node/inline reference/attach/clear chain')
        check(not f.errors and f.position==len(raw),'entire Node graph consumed')
        check(p.uint(root+0x1c)==1,'root has one actual child')
        child=p.uint(p.uint(p.uint(root+0x18))+8)
        check(child in f.allocations and p.uint(child+0x2c)==root and p.floats(child+0x74,3)==(1.,2.,3.),'child reciprocal parent and cached world')
        check(p.uint(fat+0x28)==p.uint(fat+0x34)==p.uint(fat+0x50)==0,'FAT clear leaves graph alive')
        roots.append(root);children.append(child)
    check(roots[0]!=roots[1] and children[0]!=children[1],'two simultaneous files own distinct root/child graphs')
    f.call(0x422220,this=roots[0],args=(1,))
    check(children[0] in f.freed and roots[1] not in f.freed and children[1] not in f.freed,'first root teardown does not destroy second graph')
    f.call(0x422220,this=roots[1],args=(1,));check(children[1] in f.freed,'second root releases its own child')
    f.call(0x4228a0,this=manager)
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all original Node graphs and native helper managers released')
    print(f'PASS {counters.checks}/{counters.checks}: native graph write then whole PC FFPS load/two-live teardown; not whole Save or rendering')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
