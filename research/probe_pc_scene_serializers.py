#!/usr/bin/env python3
"""Original PC RenderNode/Renderable/Model scalar section calls, bounded CPU only.

Use actual secondary vtables, not presumed unprotected writer body labels.
Renderer storage and startup identities are explicit consumer inputs.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager,empty_fat
from probe_pc_node_serializer import field

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

def main(kind,mode,return_capture=False):
    if kind not in {'render-node','renderable','model'} or mode not in {'empty','values','repeat','failed-scalar','null-target','null-links'}:
        raise ValueError('explicit bounded scene section case')
    if kind=='render-node' and mode=='failed-scalar':raise ValueError('Node failure already covered separately')
    data=b'\0'
    if kind=='render-node':
        if mode=='values':data=field(0,struct.pack('<3f',1,2,3))+b'\0'+field(9,b'X')+b'\0'
        else:data=b'\0\0'
    else:
        if mode=='null-links':data=field(0,struct.pack('<I',0))+field(1,struct.pack('<I',0))+b'\0'
        if mode=='values':data=field(2,struct.pack('<I',0xa500))+field(3,struct.pack('<I',0xdeadbeef))+b'\0'
        if mode=='repeat':data=field(2,struct.pack('<I',1))+field(2,struct.pack('<I',0))+field(3,struct.pack('<I',17))+field(3,struct.pack('<I',0xffffffff))+field(9,b'X')+b'\0'
        if mode=='failed-scalar':data=bytes([0xa2,4])
        if kind=='model' and mode!='failed-scalar':
            data+=(field(1,struct.pack('<I',0x89abcdef)) if mode=='values' else b'')+b'\0'
    f=PCWriteBytesFixture(data);p=f.p;f.call(0x6d38e0)
    # Dtor reads renderer cache/busy slots; no renderer constructor/device call.
    renderer=p.allocate(0xc9c8);p.put_uint(0x75db68,renderer)
    manager,_=empty_manager(f);fat=empty_fat(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    for record,identity,parent in ((0x755310,0x415352a1,0),(0x7555f8,0x44de07fd,0x755310),
        (0x75dd88,0x695c0f65,0x7555f8),(0x75e150,0x603625d0,0x75dd88),
        (0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    obj=f.call(0x425520 if kind=='render-node' else 0x479ed0)
    serializer=f.call({'render-node':0x469040,'renderable':0x47f650,'model':0x4934c0}[kind])
    check(f.allocations[serializer]==0x14,'actual concrete serializer exact14')
    table=p.uint(serializer+0x10);reader=p.uint(table+8);writer=p.uint(table)
    jumps={label:hex(p.uint(p.uint(entry+2))) for label,entry in (('reader',reader),('writer',writer))
           if bytes(p.mu.mem_read(entry,2))==b'\xff\x25'}
    print('SCENE ENTRIES',kind,'reader',hex(reader),'writer',hex(writer),'indirect-targets',jumps,flush=True)
    result=f.call(reader,this=serializer+0x10,args=(f.stream,0 if mode=='null-target' else obj))&255
    print('SCENE READ',kind,mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,'position',f.position,'errors',f.errors,flush=True)
    check(result==int(mode not in {'failed-scalar','null-target'}),'actual reader result')
    check(f.position==(0 if mode=='null-target' else len(data)),'section extent consumed or null rejected before input')
    if kind=='render-node':
        state=[p.uint(obj+0xb0),bytes(p.mu.mem_read(obj+0x74,60)).hex(),p.uint(obj+0xc0)-p.uint(obj+0xbc)]
        check(p.floats(obj+0x74,3)==((1.,2.,3.) if mode=='values' else (0.,0.,0.)),'inherited Node world update executed')
    else:
        state=[bytes(p.mu.mem_read(obj+0x18,1))[0],p.uint(obj+0x1c),p.uint(obj+0x5c),p.uint(obj+0x20),p.uint(obj+0x24),p.uint(obj+0x58)]
        if mode in {'values','repeat'}:
            check(state[:2]==([1,0xdeadbeef] if mode=='values' else [0,0xffffffff]),'alpha DWORD nonzero normalized, priority UInt32 preserved, repeated fields last-win')
            if kind=='model':check(state[2]==(0x89abcdef if mode=='values' else 3),'projection group UInt32/default')
    captured=[kind,mode,data.hex(),result,f.position,state,None]
    if result:
        f.data=b'';f.position=0;p.put_uint(manager+0x14,2)
        written=f.call(writer,this=serializer+0x10,args=(f.stream,obj))&255
        print('SCENE WRITE',kind,mode,'result',written,'instructions',sum(p.visits.values()),'hex',f.data.hex(),'errors',f.errors,flush=True)
        check(written==1 and not f.errors,'actual secondary writer completes')
        check(f.position==len(f.data) and f.data[-1:]==b'\0','writer final extent/terminator')
        captured[-1]=f.data.hex()
    f.call(p.uint(p.uint(obj)),this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    f.call(0x4228a0,this=manager)
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native scalar fixture allocations released')
    print('SCENE_CAPTURE',captured,flush=True)
    print(f'PASS {checks}/{checks}: original {kind}/{mode}; relationship payloads not claimed')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
