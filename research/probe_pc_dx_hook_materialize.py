#!/usr/bin/env python3
"""Whole original PC DX batch hook with tiny mesh, explicit COM/startup only."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import empty_fat,empty_manager
from probe_pc_dx_mesh_payload import MeshFixture
from probe_pc_san_reader import ReaderFixture
from probe_pc_dx_buffers import check
import probe_pc_dx_buffers as counters

def main(mode):
    if mode not in {'triangle','triangle-pair','logo-field'}:raise ValueError('explicit tiny fixture required')
    if mode=='logo-field':
        import hashlib
        raw=(ROOT/'local-data/pc-pristine/Media/Menus/logo_screen.smo').read_bytes()
        if hashlib.sha256(raw).hexdigest().upper()!='DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7':
            raise AssertionError('unchanged small corpus fixture required')
        payload=raw[484:484+193]
        expected_vertex=raw[533:677];expected_index=raw[513:521]
        header=(0x940,4,144,8,0);flags=0x940
    else:
        expected_vertex=struct.pack('<24f',*range(24));expected_index=struct.pack('<3H',0,1,2)
        flags=0x840;header=(0x112,3,96,6,0)
        payload=struct.pack('<IIIIB',*header)+struct.pack('<III',2,1,0)+expected_index+struct.pack('<III',flags,3,0)+expected_vertex
    if len(payload)>255:raise AssertionError('tiny compact field capacity')
    body=struct.pack('<II',0x33c34cf0,0x4f4f4253)+bytes([0xa1,len(payload)])+payload+b'\0'
    name=b'bounded-mesh\0';count=2 if mode=='triangle-pair' else 1
    directory=struct.pack('<I',count)+b''.join(struct.pack('<IH',7+i,len(name))+name+
        struct.pack('<III',0x33c34cf0,i*len(body),len(body)) for i in range(count))
    f=MeshFixture(directory+body*count);p=f.p
    p.seams[0x4130f0]=lambda p:ReaderFixture.set_name(f,p)
    p.seams[0x4173e0]=lambda p:ReaderFixture.release_name(f,p)
    # Seed original RTTI records; do not replace IsKindOf or force its result.
    for record,identity,parent in ((0x763150,0x193b2671,0x763b40),(0x763b40,0x67974a9c,0x75e090),
            (0x75e090,0x3f077b6c,0x7603a0),(0x7603a0,0x46f043fe,0x7555f8),
            (0x7555f8,0x44de07fd,0x755310),(0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    # FAT validates the wire class through its original RTTI membership call.
    # Cold RTTI construction is already capped/unresolved: supply the same
    # explicit one-entry startup tree as earlier FAT probes, never resume it.
    rtti=p.allocate(0x20);head=p.allocate(24);node=p.allocate(24)
    for offset in (0,4,8):p.put_uint(head+offset,node);p.put_uint(node+offset,head)
    p.mu.mem_write(head+20,b'\x01\x01');p.mu.mem_write(node+20,b'\x01\x00')
    p.put_uint(node+12,0x33c34cf0);p.put_uint(node+16,0x75d428)
    p.put_uint(rtti+0x18,head);p.put_uint(rtti+0x1c,1);p.put_uint(0x755378,rtti)
    p.put_uint(0x75d428,0x33c34cf0);p.put_uint(0x75d428+0x48,0x75e090)
    fat=empty_fat(f);manager,_=empty_manager(f)
    p.put_uint(manager+0x28,fat);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x4297c0)
    check(f.allocations[serializer]==0x14 and p.uint(serializer)==0x6dccb8,'actual DXMeshDataSerializer factory exact14')
    f.call(0x422d90,this=manager,args=(0x33c34cf0,serializer,2,1))
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual FAT loaded')
    p.put_uint(f.stream+0x14,len(directory));f.io.clear()
    entries=[f.call(0x4664c0,this=fat,args=(7+i,)) for i in range(count)];entry=entries[0]
    publications=[];create_declaration=p.seams[0x34060090]
    def observe_create(p):
        publications.append([p.uint(e+0x20) for e in entries]);create_declaration(p)
    p.seams[0x34060090]=observe_create
    result=f.call(0x4aa870,args=(fat,f.stream))&255;visited=set(p.visits)
    mesh=p.uint(entry+0x20)
    print('HOOK',mode,'result',result,'mesh',hex(mesh),'instructions',sum(p.visits.values()),
          'heap',p.allocated,'position',f.position,'events',f.events,'errors',f.errors,flush=True)
    check(result==1 and mesh in f.allocations,'complete original batch hook materializes entry')
    check(p.uint(mesh)==0x6ef334 and p.uint(mesh+0x44)==flags,'wire MeshData becomes actual DXMesh with component flags')
    check(all(a in visited for a in (0x4aa4e0,0x4a96c0,0x4224f0,0x42afd0,0x429bc0,0x429a40,0x4ae0e0,0x408370,0x4130f0)),
          'original metadata, batch, registry, header, fields, materialization, declaration and name dispatch')
    check(p.uint(0x763148)==0 and not f.errors,'normal return clears active combiner without diagnostic')
    vb=p.uint(mesh+0x58);ib=p.uint(mesh+0x54)
    check(bytes(p.mu.mem_read(f.buffers[p.uint(vb+0x10)]['data'],len(expected_vertex)))==expected_vertex,'exact whole-hook vertex bytes')
    check(bytes(p.mu.mem_read(f.buffers[p.uint(ib+0x10)]['data'],len(expected_index)))==expected_index,'exact whole-hook index bytes')
    check(p.uint(mesh+0x84) in f.declaration_objects(),'actual declaration object stored')
    check(publications==[[0]*count],'first declaration creation precedes FAT publication of either mesh')
    meshes=[p.uint(e+0x20) for e in entries]
    if count==2:
        second=meshes[1]
        check(second!=mesh and second in f.allocations,'two distinct live DX mesh objects')
        check(p.uint(second+0x54)==ib and p.uint(second+0x58)==vb,'both meshes share one native buffer pair')
        check(p.uint(second+0x78)==3 and p.uint(second+0x7c)==3,'second mesh starts after first index/vertex range')
        check(p.uint(second+0x84)==p.uint(mesh+0x84) and len(f.declaration_arrays)==1,'two meshes reuse one actual declaration')
        check(bytes(p.mu.mem_read(f.buffers[p.uint(vb+0x10)]['data'],len(expected_vertex)*2))==expected_vertex*2,'both vertex ranges copied exactly')
        check(bytes(p.mu.mem_read(f.buffers[p.uint(ib+0x10)]['data'],len(expected_index)*2))==expected_index*2,'second index bytes remain local, not rebased in storage')
        check(p.uint(vb+8)&0xffff==3 and p.uint(ib+8)&0xffff==3,'two meshes plus combiner each retain wrappers')
    combiners=[a for a,s in f.allocations.items() if a not in f.freed and s==0x2c and p.uint(a)==0x6ef294]
    check(len(combiners)==1,'normal hook leaves allocated combiner alive; no invented ownership/cleanup')
    # Explicit harness cleanup of surviving objects, NOT a native hook rollback.
    f.call(0x466760,this=fat)
    for current in meshes:f.call(0x4aa350,this=current,args=(1,))
    for combiner in combiners:f.call(0x4a9f30,this=combiner,args=(1,))
    f.clear_declarations();f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(all(b['refs']==0 for b in f.buffers.values()),'fixture-owned COM storage fully released')
    check(set(f.allocations)==set(f.freed),'all native allocations released by explicit final fixture cleanup')
    print(f'PASS {counters.checks}/{counters.checks}: whole PC DX hook {mode}; not whole FFPS/GPU evidence')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
