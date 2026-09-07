#!/usr/bin/env python3
"""Whole original PC generic FFPS loader with an empty animation and one mesh.

The FFPS envelope is an explicit tiny test input, NOT a whole Save recovery.
RTTI/container startup and COM are declared fixtures; no host OS/GPU calls.
Uses the unchanged per-call100k/2s and64KiB arena/30s-child limits.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import empty_fat,empty_manager,PCFileBytesFixture
from probe_pc_dx_mesh_payload import MeshFixture
from probe_pc_san_reader import ReaderFixture
from probe_pc_read_reference import EMPTY_OBJECT
from probe_pc_dx_buffers import check
import probe_pc_dx_buffers as counters


def input_file():
    vertex=struct.pack('<24f',*range(24));index=struct.pack('<3H',0,1,2)
    payload=struct.pack('<IIIIB',0x112,3,96,6,0)+struct.pack('<III',2,1,0)+index+struct.pack('<III',0x840,3,0)+vertex
    mesh=struct.pack('<II',0x33c34cf0,0x4f4f4253)+bytes([0xa1,len(payload)])+payload+b'\0'
    name=b'whole-mesh\0'
    directory=struct.pack('<I',2)+struct.pack('<IHIII',1,0,0x56ee563a,0,len(EMPTY_OBJECT))
    directory+=struct.pack('<IH',2,len(name))+name+struct.pack('<III',0x33c34cf0,len(EMPTY_OBJECT),len(mesh))
    origin=28+len(directory)+4;body=EMPTY_OBJECT+mesh
    return struct.pack('<7I',0x53504646,0x26,0,origin+len(body),2,origin,len(body))+directory+struct.pack('<I',0)+body,vertex,index,origin


def startup_rtti(f):
    # Valid two-entry RB tree. Wire IDs, parent records and the Animation
    # factory come from separate executable-backed registration evidence.
    p=f.p;rtti=p.allocate(0x20);head=p.allocate(24);mesh=p.allocate(24);animation=p.allocate(24)
    for node in (mesh,animation):
        for offset in (0,4,8):p.put_uint(node+offset,head)
    p.put_uint(head,mesh);p.put_uint(head+4,mesh);p.put_uint(head+8,animation)
    p.put_uint(mesh+8,animation);p.put_uint(animation+4,mesh)
    p.mu.mem_write(head+20,b'\x01\x01');p.mu.mem_write(mesh+20,b'\x01\x00');p.mu.mem_write(animation+20,b'\x00\x00')
    for node,identity,record in ((mesh,0x33c34cf0,0x75d428),(animation,0x56ee563a,0x75d248)):
        p.put_uint(node+12,identity);p.put_uint(node+16,record)
    p.put_uint(rtti+0x18,head);p.put_uint(rtti+0x1c,2);p.put_uint(0x755378,rtti)
    p.put_uint(0x75d248+0x4c,0x41a090)
    for record,identity,parent in ((0x75d248,0x56ee563a,0x75de50),
            (0x75de50,0x4fad24f1,0x760340),(0x760340,0x062c22ed,0x755310),
            (0x75d428,0x33c34cf0,0x75e090),(0x763150,0x193b2671,0x763b40),
            (0x763b40,0x67974a9c,0x75e090),(0x75e090,0x3f077b6c,0x7603a0),
            (0x7603a0,0x46f043fe,0x7555f8),(0x7555f8,0x44de07fd,0x755310),
            (0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)


def main():
    raw,vertex,index,origin=input_file();f=MeshFixture(raw);p=f.p
    p.put_uint(p.uint(f.stream)+0x3c,0x340600c0)
    p.seams[0x340600c0]=lambda p:PCFileBytesFixture.get_size(f,p)
    p.seams[0x4130f0]=lambda p:ReaderFixture.set_name(f,p)
    p.seams[0x4173e0]=lambda p:ReaderFixture.release_name(f,p)
    startup_rtti(f);fat=empty_fat(f);manager,_=empty_manager(f)
    p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
    animation_serializer=f.call(0x43dab0);mesh_serializer=f.call(0x4297c0)
    f.call(0x422d90,this=manager,args=(0x56ee563a,animation_serializer,0xffffffff,1))
    f.call(0x422d90,this=manager,args=(0x33c34cf0,mesh_serializer,2,1))
    root=f.call(0x422b50,this=manager,args=(f.stream,));visited=set(p.visits)
    print('WHOLE PC FFPS',hex(root),'instructions',sum(p.visits.values()),'heap',p.allocated,
          'position',f.position,'length',len(raw),'errors',f.errors,flush=True)
    check(root in f.allocations and p.uint(root)==0x6de6cc,'outer loader returns actual Animation root, not prebound mesh')
    for entry in (0x422260,0x466b90,0x465cd0,0x4aa430,0x4aab80,0x4aa870,0x4aa4e0,
                  0x4224f0,0x42afd0,0x429bc0,0x429a40,0x4ae0e0,0x422940,0x467550,
                  0x414420,0x43ecc0,0x466760):
        check(entry in visited,f'whole load executes original {entry:08X}')
    meshes=[a for a,s in f.allocations.items() if a not in f.freed and s==0x88 and p.uint(a)==0x6ef334]
    check(len(meshes)==1,'exactly one live DX mesh survives FAT clear')
    mesh=meshes[0];vb=p.uint(mesh+0x58);ib=p.uint(mesh+0x54)
    check(p.uint(mesh+0x44)==0x840 and p.uint(mesh+0x84) in f.declaration_objects(),'actual geometry and declaration stored')
    check(bytes(p.mu.mem_read(f.buffers[p.uint(vb+0x10)]['data'],len(vertex)))==vertex,'whole loader exact vertex bytes')
    check(bytes(p.mu.mem_read(f.buffers[p.uint(ib+0x10)]['data'],len(index)))==index,'whole loader exact index bytes')
    check(not f.errors and p.uint(f.stream+0x14)==origin,'no diagnostic and correct logical data origin')
    check(all(op[2]==op[3] for op in f.io if op[0]=='read'),'valid input never relies on short reads')
    check(p.uint(fat+0x28)==p.uint(fat+0x34)==p.uint(fat+0x50)==0 and p.uint(fat+0x10)==1,'native whole loader clears index but leaves objects alive')
    check(p.uint(0x763148)==0,'active native combiner cleared on normal return')
    combiners=[a for a,s in f.allocations.items() if a not in f.freed and s==0x2c and p.uint(a)==0x6ef294]
    check(len(combiners)==1,'normal load still leaves combiner lifetime unresolved')
    # Explicit fixture cleanup, not a claim of native whole-scene ownership.
    f.call(p.uint(p.uint(root)),this=root,args=(1,));f.call(0x4aa350,this=mesh,args=(1,))
    for combiner in combiners:f.call(0x4a9f30,this=combiner,args=(1,))
    f.clear_declarations();f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(all(b['refs']==0 for b in f.buffers.values()),'all isolated COM buffers released')
    check(set(f.allocations)==set(f.freed),'all tracked native allocations released by explicit fixture cleanup')
    print(f'PASS {counters.checks}/{counters.checks}: original whole PC FFPS load with nonempty mesh hook; no GPU/scene ownership claim')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
