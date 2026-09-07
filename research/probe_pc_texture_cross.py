#!/usr/bin/env python3
"""Tiny cross-platform texture fields through original PC CPU data object."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager
from probe_pc_node_serializer import field

def main(mode,return_capture=False):
    if mode not in {'rgba','gray','rgb16'}:raise ValueError('small typed pixel fixture')
    checks=0
    def check(ok,text):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(text)
    fmt,pixel_size={'rgba':(0,4),'gray':(2,1),'rgb16':(3,2)}[mode]
    width,height=2,1;pixels=bytes(range(1,width*height*pixel_size+1))
    nested=field(5,struct.pack('<4I',width,height,fmt,pixel_size)+pixels)+b'\0'
    # Source wrapper is its own terminated section before the local payload.
    data=field(2,b'\0')+b'\0'+field(6,struct.pack('<I',1))+field(0,nested)+b'\0'
    f=PCWriteBytesFixture(data);p=f.p;manager,_=empty_manager(f)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    obj=f.call(0x41a2d0);serializer=f.call(0x42dc30)
    result=f.call(0x42f180,this=serializer+0x10,args=(f.stream,obj))&255
    print('TEXTURE_CROSS_READ',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'errors',f.errors,flush=True)
    check(result==1 and f.position==len(data) and not f.errors,'actual complete CPU texture read')
    buffer=obj+0x38;state=[p.uint(buffer+i) for i in (0x10,0x14,0x18,0x20,0x28)]
    actual=bytes(p.mu.mem_read(p.uint(buffer+0x1c),len(pixels)))
    check(state==[width,height,1,fmt,pixel_size] and actual==pixels,'native CPU buffer header and pixels')
    f.data=b'';f.position=0;p.put_uint(manager+0x14,2)
    result=f.call(0x42ee10,this=serializer+0x10,args=(f.stream,obj))&255
    print('TEXTURE_CROSS_WRITE',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),'errors',f.errors,flush=True)
    check(result==1 and not f.errors,'actual CPU texture writer')
    captured=[mode,data.hex(),state,actual.hex(),f.data.hex()]
    f.call(p.uint(p.uint(obj)),this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'all temporary/source CPU texture allocations freed')
    print('TEXTURE_CROSS_CAPTURE',captured,flush=True);print(f'PASS {checks}/{checks}: actual PC CPU cross texture {mode}')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
