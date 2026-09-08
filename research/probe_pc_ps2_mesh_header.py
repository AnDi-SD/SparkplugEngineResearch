#!/usr/bin/env python3
"""Observe the original PS2-mesh header prefix; never execute a DMA packet."""
from pathlib import Path
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import empty_manager
from probe_pc_dx_mesh_payload import MeshFixture
from inspect_serializer_manager import read_elf,image_slice,PS2_SHA256,sha256

def main(mode,output):
    if mode not in ('header','raw-counters','bounds'):raise ValueError('Explicit bounded mode required')
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    header=struct.pack('<4f6I',1,2,3,4,2,4,0x197e,1,1,4)
    if mode=='raw-counters':header=struct.pack('<4f6I',-1,2,3,-4,0,999,0,0,7,2)
    bounds=struct.pack('<6f',3,4,5,-1,-2,-3) # reversed values are still copied
    payload=b'\xa2\x18'+bounds+b'\0' if mode=='bounds' else header
    f=MeshFixture(payload);p=f.p;mesh=f.call(0x4a9e80)
    serializer=manager=None
    if mode=='bounds':
        manager,_=empty_manager(f);p.put_uint(manager+0x10,2);p.put_uint(0x75dde8,manager)
        serializer=f.call(0x42a2a0)
        returned=f.call(0x42ab40,this=serializer+0x10,args=(f.stream,mesh))&255
        assert returned==1 and f.position==len(payload) and not f.errors
        observed=bytes(p.mu.mem_read(mesh+0x2c,24));assert observed==bounds
        complete=True
    else:
        # All metadata reads complete before the first packet allocation.
        p.run(0x42a420,args=(f.stream,mesh),stop_at=0x42a4b1)
        assert p.reg('EIP')==0x42a4b1 and f.position==40 and not f.errors
        sp=p.reg('ESP')
        values=[p.uint(sp+offset) for offset in (0x18,0x1c,0x60,0x64,0x20,0x24)]
        observed=bytes(p.mu.mem_read(mesh+0x18,16))+struct.pack('<6I',*values)
        assert observed==header
        # Explicitly abandon the stopped frame; it is never resumed as success.
        p.put_uint(f.teb,0xffffffff);complete=False
    instructions=sum(p.visits.values());heap=p.allocated
    f.call(0x4aa350,this=mesh,args=(1,));f.clear_declarations()
    if serializer:f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    if manager:f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    assert all(b['refs']==0 for b in f.buffers.values()) and set(f.allocations)==set(f.freed)
    ps2=(ROOT/'local-data/Winx Club the game PS2/SLES_532.19').read_bytes();assert sha256(ps2)==PS2_SHA256
    sections=read_elf(ps2);prefix=image_slice(ps2,sections,0x163160,0x88)
    # Read16 to mesh+18 followed by six UInt32 stream helpers; independent PS2 code.
    assert struct.unpack_from('<I',prefix,0x30)[0]==0x26450018
    assert struct.unpack_from('<I',prefix,0x3c)[0]==0x24060010
    calls=[word for word, in struct.iter_unpack('<I',prefix) if word>>26==3]
    assert calls==[0x0c04538c]*6 # JAL00114E30
    record={'status':'passed','mode':mode,'input_hex':payload.hex(),'observed_hex':observed.hex(),
        'complete_whole_field_reader':complete,'stop_pc':None if complete else '0042A4B1',
        'instructions_before_cleanup':instructions,'arena_bytes':heap,'allocations':len(f.allocations),'freed':len(f.freed),
        'pc_sha256':sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()),
        'ps2_sha256':PS2_SHA256,'ps2_prefix_sha256':sha256(prefix),
        'probe_sha256':sha256(Path(__file__).read_bytes()),
        'scope':'PC original metadata prefix or complete bounds-only field reader; PS2 prefix static corroboration. No packet allocation/attach, DMA execution, or full native mesh load claim.'}
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    print('PASS PS2-mesh metadata',mode,'instructions',instructions,'allocations',len(f.allocations),'/',len(f.freed));return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
