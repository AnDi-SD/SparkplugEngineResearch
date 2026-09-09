#!/usr/bin/env python3
"""Original Model/Renderable scalar sections, no mesh/resource substitutes."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_relationships import node_rtti
from probe_pc_node_serializer import field

MODES=('empty','priority-only','alpha-wide','repeat')
def specimen(mode):
    if mode=='empty':return b'\0\0'
    if mode=='priority-only':return field(3,struct.pack('<I',7))+b'\0\0'
    if mode=='alpha-wide':return field(2,struct.pack('<I',256))+b'\0\0'
    if mode=='repeat':return (field(3,struct.pack('<I',0xffffffff))+field(2,struct.pack('<I',2))+
        field(18,b'ignored')+field(0,struct.pack('<I',0))+field(1,struct.pack('<I',0))+
        field(2,struct.pack('<I',0))+b'\0'+field(1,struct.pack('<I',9))+
        field(17,b'tail')+field(1,struct.pack('<I',5))+b'\0')
    raise ValueError('explicit small Model mode')
def main(mode):
    data=specimen(mode);f=PCWriteBytesFixture(data);p=f.p;node_rtti(f)
    for record,identity,parent in ((0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    model=f.call(0x479ed0);serializer=f.call(0x4934c0)
    assert f.allocations[model]==0x60 and f.allocations[serializer]==0x14
    result=f.call(0x4938f0,this=serializer+0x10,args=(f.stream,model))&255
    assert result==1 and f.position==len(data) and not f.errors
    state=[bytes(p.mu.mem_read(model+0x18,1))[0],p.uint(model+0x1c),p.uint(model+0x5c)]
    assert p.uint(model+0x20)==p.uint(model+0x24)==p.uint(model+0x58)==0
    f.data=b'';f.position=0
    assert f.call(0x4935f0,this=serializer+0x10,args=(f.stream,model))&255==1 and not f.errors
    capture=[mode,data.hex(),state,f.data.hex()]
    f.call(0x479f90,this=model,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    assert set(f.allocations)==set(f.freed)
    print('MODEL_FIELDS_CAPTURE',json.dumps(capture),flush=True)
    print('PASS original Model fields',mode,'heap',p.allocated,flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
