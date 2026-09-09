#!/usr/bin/env python3
"""Actual typed FAT prebound navigation refs and list/ID side effects."""
from pathlib import Path
import sys,struct,json
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_navigation_readers import initialize,cleanup,SPECS
from probe_pc_node_serializer import field
def ref(i):return struct.pack('<II',i,0)
def main(kind):
    f=PCWriteBytesFixture();p=f.p;rows=initialize(f)
    kinds={'graph':0x188a161f,'set':0x7297173c,'portal':0x385662aa}
    records={row['class_hash']:row for row in rows if row['class_hash'] in kinds.values()}
    # Three-entry valid RB tree is explicit startup state; every identity/factory
    # is from original registrations. Actual lookup and FAT parsing execute.
    rtti=p.uint(0x755378);head=p.uint(rtti+0x18);oldroot=p.uint(head+4)
    nodes=[p.allocate(24),oldroot,p.allocate(24)]
    identities=sorted(kinds.values())
    for i,(node,identity) in enumerate(zip(nodes,identities)):
        for offset in (0,4,8):p.put_uint(node+offset,head)
        p.put_uint(node+12,identity);p.put_uint(node+16,records[identity]['registration_object_va'])
        p.mu.mem_write(node+20,bytes([1 if i==1 else 0,0]))
    p.put_uint(nodes[1],nodes[0]);p.put_uint(nodes[1]+8,nodes[2]);p.put_uint(nodes[0]+4,nodes[1]);p.put_uint(nodes[2]+4,nodes[1])
    for offset,node in ((0,nodes[0]),(4,nodes[1]),(8,nodes[2])):p.put_uint(head+offset,node)
    p.put_uint(rtti+0x1c,3)
    needed={'graph':[(7,'set'),(9,'set'),(11,'portal')],'set':[(7,'portal')],'portal':[(7,'graph'),(9,'set'),(11,'set')]}[kind]
    refs={};objects=[]
    for identity,target in needed:
        obj=f.call(SPECS[target][0]);refs[identity]=obj;objects.append(obj)
        p.put_uint(obj+8,(p.uint(obj+8)&0xffff0000)|1) # explicit external lifetime
    f.data=struct.pack('<I',len(needed))+b''.join(struct.pack('<IHIII',identity,0,kinds[target],0,0) for identity,target in needed);f.position=0
    assert f.call(0x466b90,this=f.navigation_fat,args=(f.stream,))&255
    for identity,obj in refs.items():p.put_uint(f.call(0x4664c0,this=f.navigation_fat,args=(identity,))+0x20,obj)
    obj=f.call(SPECS[kind][0]);serializer=f.call(SPECS[kind][1]);reader=p.uint(p.uint(serializer+0x10)+8)
    if kind=='graph':wire=b'\0\0'+field(0,ref(7))+field(0,ref(9))+field(0,ref(7))+field(1,ref(11))+field(1,ref(11))+b'\0'
    elif kind=='set':wire=b'\0'+field(4,ref(7))+field(4,ref(7))+b'\0\0'
    else:wire=b'\0'+field(0,ref(7))+field(1,ref(9)+ref(11))+b'\0'
    f.data=wire;f.position=0
    result=f.call(reader,this=serializer+0x10,args=(f.stream,obj))&255
    assert result==1 and f.position==len(wire) and not f.errors,(result,f.position,f.errors)
    def key(pointer):return 0 if not pointer else 1 if pointer==obj else next(i for i,v in refs.items() if v==pointer)
    def pointers(offset):return [key(p.uint(address)) for address in range(p.uint(obj+offset+4),p.uint(obj+offset+8),4)]
    state={'refs':{str(i):p.uint(pointer+8)&65535 for i,pointer in refs.items()}}
    if kind=='graph':state.update(sets=pointers(0x1d8),portals=pointers(0x1e8),set_ids=[bytes(p.mu.mem_read(refs[i]+0xe0,1))[0] for i in (7,9)],portal_id=bytes(p.mu.mem_read(refs[11]+0xf1,1))[0],portal_graph_untouched=p.uint(refs[11]+0xb4)==0xcccccccc)
    elif kind=='set':state.update(portals=pointers(0xc8))
    else:state.update(graph=key(p.uint(obj+0xb4)),sets=[key(p.uint(obj+offset)) for offset in (0xb8,0xbc)])
    print('NAV_LINK_CAPTURE',json.dumps({'kind':kind,'wire':wire.hex(),'state':state,'arena':p.allocated}),flush=True)
    cleanup(f,[obj,serializer,*objects]);print('PASS navigation borrowed relationships',kind,flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
