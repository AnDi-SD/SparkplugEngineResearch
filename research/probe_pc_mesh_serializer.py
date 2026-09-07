#!/usr/bin/env python3
"""Complete original MeshData/DXMeshData field readers on bounded tiny inputs."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import empty_manager
from probe_pc_dx_mesh_payload import MeshFixture
from probe_pc_dx_buffers import check
import probe_pc_dx_buffers as counters

def main(mode,return_capture=False):
    if mode not in {'base-cross','dx-cross','dx-native','dx-both','dx-packed','dx-unknown'}:
        raise ValueError('explicit bounded mesh serializer mode required')
    packed=mode=='dx-packed';flags=0x20 if packed else 0x840
    vertices=(b''.join(struct.pack('<fff4B',float(i),float(i+1),float(i+2),1,2,3,255) for i in range(3))
              if packed else struct.pack('<24f',*range(24)))
    wanted=(b''.join(struct.pack('<7f',float(i),float(i+1),float(i+2),1,2,3,255) for i in range(3))
            if packed else vertices)
    indices=struct.pack('<3H',0,1,2)
    cross=struct.pack('<III',2,1,0)+indices+struct.pack('<III',flags,3,0)+vertices
    native=struct.pack('<IIIIB',0xdeadbeef,999,777,555,0xa5)+cross # actual helper ignores values
    fields=(bytes([0xa0,len(cross)])+cross if mode.endswith('cross') else bytes([0xa1,len(native)])+native)
    if mode=='dx-both':fields=bytes([0xa0,len(cross)])+cross+fields
    if mode=='dx-unknown':fields=b'\x22X'+fields
    # Specialised native header intentionally ignores identity and marker.
    data=struct.pack('<II',0x12345678,0xdeadbeef)+fields+b'\0'
    f=MeshFixture(data);p=f.p;manager,_=empty_manager(f)
    p.put_uint(manager+0x10,1 if mode=='dx-cross' else 2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x42aef0 if mode=='base-cross' else 0x4297c0)
    check(f.allocations[serializer]==0x14,'actual concrete serializer factory exact14')
    mesh=f.call(0x42afd0,this=serializer,args=(f.stream,))
    check(mesh in f.allocations and p.uint(mesh)==0x6ef334,'specialised header ignores both words and returns actual DXMesh')
    result=f.call(0x42b420 if mode=='base-cross' else 0x429bc0,this=serializer+0x10,args=(f.stream,mesh))&255
    print('MESH SERIALIZER',mode,result,'position',f.position,'instructions',sum(p.visits.values()),
          'heap',p.allocated,'meshwords',f.words(mesh,0x44,0x88),'events',f.events,'errors',f.errors,flush=True)
    check(result==1 and f.position==len(data) and not f.errors,'complete fields consumed normally')
    check(0x4aa000 in p.visits and 0x4ae0e0 in p.visits and p.uint(0x763148)==0,'standalone mesh path/declaration, no active combiner')
    check(len(f.declaration_arrays)==1,'selected geometry initialized exactly once including field0/1 coexistence')
    vb=p.uint(mesh+0x58);ib=p.uint(mesh+0x54)
    check(bytes(p.mu.mem_read(f.buffers[p.uint(vb+0x10)]['data'],len(wanted)))==wanted,'standalone exact converted vertex bytes')
    check(bytes(p.mu.mem_read(f.buffers[p.uint(ib+0x10)]['data'],len(indices)))==indices,'standalone exact index bytes')
    check(p.uint(mesh+0x64)==len(wanted),'standalone stored vertex size has no combiner double-add')
    check(p.uint(mesh+0x44)==flags and p.uint(mesh+0x74)==(28 if packed else 32),'component flags and expanded stride')
    check(p.uint(vb+8)&0xffff==1 and p.uint(ib+8)&0xffff==1,'standalone wrappers owned only by mesh')
    captured=[mode,data.hex(),f.position,p.uint(mesh+0x70),p.uint(mesh+0x74),
              p.uint(mesh+0x64),p.uint(mesh+0x60),p.uint(mesh+0x80),
              bytes(p.mu.mem_read(f.buffers[p.uint(vb+0x10)]['data'],len(wanted))).hex(),
              bytes(p.mu.mem_read(f.buffers[p.uint(ib+0x10)]['data'],len(indices))).hex()]
    f.call(0x4aa350,this=mesh,args=(1,));f.clear_declarations()
    f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(all(b['refs']==0 for b in f.buffers.values()) and set(f.allocations)==set(f.freed),'normal native object teardown releases all tracked storage')
    print(f'PASS {counters.checks}/{counters.checks}: original mesh serializer {mode}; standalone COM fixture, not whole-file/GPU')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
