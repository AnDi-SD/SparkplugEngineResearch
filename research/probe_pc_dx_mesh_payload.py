#!/usr/bin/env python3
"""Actual PC native-field->CPU buffers->DX copy, stop before unknown declaration map."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture
from probe_pc_san_reader import ReaderFixture
from probe_pc_dx_buffers import check
from pc_declaration_fixture import DeclarationDeviceFixture
import probe_pc_dx_buffers as counters

class MeshFixture(DeclarationDeviceFixture):
    def __init__(self,data):
        super().__init__('mesh');self.data=data;self.position=0;self.io=[];self.errors=[]
        p=self.p;self.stream=p.allocate(0x20);vt=p.allocate(0x40);p.put_uint(self.stream,vt)
        for offset,address,method in ((0x28,0x34060060,PCFileBytesFixture.seek),
                (0x2c,0x34060070,PCFileBytesFixture.tell),(0x30,0x34060080,PCFileBytesFixture.read)):
            p.put_uint(vt+offset,address);p.seams[address]=lambda p,m=method:m(self,p)
        p.seams[0x413540]=lambda p:ReaderFixture.error_message(self,p)

def main(mode):
    if mode not in {'triangle','ignored-header','index32','packed','triangle-complete','packed-complete'}:
        raise ValueError('explicit bounded mesh payload mode required')
    complete=mode.endswith('-complete');packed=mode.startswith('packed');index32=mode=='index32'
    flags=0x20 if packed else 0x840
    vertices=(b''.join(struct.pack('<fff4B',float(i),float(i+1),float(i+2),1,2,3,255) for i in range(3))
              if packed else struct.pack('<24f',*range(24)))
    expected_vertices=(b''.join(struct.pack('<7f',float(i),float(i+1),float(i+2),1,2,3,255) for i in range(3))
                       if packed else vertices)
    indices=struct.pack('<3I' if index32 else '<3H',0,1,2)
    fvf=0x1002 if packed else 0x112
    header=(0xdeadbeef,999,777,555,0xa5) if mode=='ignored-header' else (fvf,3,len(expected_vertices),len(indices),int(index32))
    # The native helper discards its17byte planning header then reads ordinary
    # serialized INDEX first, VERTEX second. NOT raw VB followed by raw IB.
    data=struct.pack('<IIIIB',*header)+struct.pack('<III',2,1,int(index32))+indices+struct.pack('<III',flags,3,0)+vertices
    f=MeshFixture(data);p=f.p
    mesh=f.call(0x4a9e80)
    print('MESH FACTORY',hex(mesh),'size',f.allocations[mesh],flush=True)
    check(f.allocations[mesh]==0x88 and p.uint(mesh)==0x6ef334,'actual DX mesh factory extent and table')
    combiner=p.allocate(0x2c);f.call(0x4a9610,this=combiner)
    check(f.call(0x4a96c0,this=combiner,args=(fvf,3,len(expected_vertices),len(indices),0))&255==1,'actual combiner prepares bounded COM storage')
    p.put_uint(0x763148,combiner)
    p.run(0x429a40,args=(f.stream,mesh),stop_at=None if complete else 0x4aa336)
    if complete:
        check(p.reg('EAX')&255==1,'complete native mesh helper returns true, including actual declaration map/create')
        check(p.uint(mesh+0x84) in f.declaration_objects(),'mesh stores actual PC declaration pointer, not FVF numeric handle')
    else:check(p.reg('EIP')==0x4aa336,'explicit stop before declaration lookup for isolated copy evidence')
    # Packed conversion overwrites EBX with vertex count before this boundary.
    # Locate the two live actual CPU factory objects by exact size and vtable.
    vertex_objects=[a for a,s in f.allocations.items() if a not in f.freed and s==0x5c and p.uint(a)==0x6e75bc]
    index_objects=[a for a,s in f.allocations.items() if a not in f.freed and s==0x28 and p.uint(a)==0x6e73b8]
    check(len(vertex_objects)==len(index_objects)==(0 if complete else 1),'native helper releases CPU temporaries on normal return; stopped frame retains them')
    print('MESH PAYLOAD',mode,'position',f.position,'instructions',sum(p.visits.values()),
          'meshwords',f.words(mesh,0x44,0x88),'events',f.events,flush=True)
    check(f.position==len(data) and not f.errors,'native header and both CPU serialized buffers consumed')
    check(all(a in p.visits for a in (0x45fb80,0x460300,0x4aa000,0x4a98e0,0x4b21e0)),'original CPU readers, DX copy, commit and FVF conversion executed')
    vb=p.uint(mesh+0x58);ib=p.uint(mesh+0x54)
    vb_bytes=f.buffers[p.uint(vb+0x10)];ib_bytes=f.buffers[p.uint(ib+0x10)]
    check(bytes(p.mu.mem_read(vb_bytes['data'],len(expected_vertices)))==expected_vertices,'exact copied/expanded vertex bytes')
    check(bytes(p.mu.mem_read(ib_bytes['data'],len(indices)))==indices,'exact16/32bit index bytes despite hardwired INDEX16 COM format')
    check(p.uint(vb+8)&0xffff==2 and p.uint(ib+8)&0xffff==2,'mesh and combiner each retain common wrappers')
    check(p.uint(mesh+0x78)==p.uint(mesh+0x7c)==0,'first mesh range begins at zero')
    check(p.uint(mesh+0x64)==len(expected_vertices)+(36 if packed else 0),
          'packed combiner path double-adds12 bytes per vertex to stored byte size; actual copied bytes differ')
    check([e for e in f.events if e[0]=='unlock']==[('unlock','vertex'),('unlock','index')],'completed copy unlocks shared buffers')
    # Abort this explicitly stopped frame, do not resume or synthesize success.
    p.put_uint(f.teb,0xffffffff);p.put_uint(0x763148,0)
    for obj in vertex_objects+index_objects:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    f.call(0x4aa350,this=mesh,args=(1,));f.call(0x4a9640,this=combiner)
    f.clear_declarations()
    resources=p.uint(0x75db78)
    if resources:f.call(p.uint(p.uint(resources)),this=resources,args=(1,))
    check(all(b['refs']==0 for b in f.buffers.values()),'all COM fixture resources released after explicit frame abort')
    check(set(f.allocations)==set(f.freed),'all tracked native objects/buffers released after abort')
    print(f'PASS {counters.checks}/{counters.checks}: original DX mesh payload {mode}; complete helper={complete}, COM fixture not whole-loader/GPU proof')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
