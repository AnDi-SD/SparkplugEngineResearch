#!/usr/bin/env python3
"""Actual DX data writer consumes declared CPU TextureData mip-vector storage.

Factory/scalar setters/serializer and destructor run original instructions.
Vector records/pixels are explicit small caller-owned input; not a reconstruction
claim for vector creation/conversion. Policy1 disables separate cross payload.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager

def main(mode,return_capture=False):
    if mode not in ('raw','raw-2','dxt1-4','raw-both','raw-auto','raw-4-partial'):raise ValueError('six tiny native writer cases')
    parts=mode.split('-');size=int(parts[1]) if len(parts)>1 and parts[1].isdigit() else 1;flags=1 if parts[0]=='dxt1' else 0
    policy=2 if mode=='raw-both' else 0 if mode=='raw-auto' else 1
    f=PCWriteBytesFixture();p=f.p;manager,_=empty_manager(f)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,2);p.put_uint(manager+0x18,policy);p.put_uint(0x75dde8,manager)
    texture=f.call(0x41a2d0);serializer=f.call(0x42b660);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    for entry,value in ((0x435000,1),(0x434fa0,size),(0x434fb0,size),(0x434f80,flags),(0x434fe0,1)):
        f.call(entry,this=texture,args=(value,))
    if policy!=1:
        f.call(0x475f30,this=texture+0x38,args=(size,size,1,0,0))
        p.mu.mem_write(p.uint(texture+0x54),b'\x01\x02\x03\x04')
    layouts=[];dimension=size
    while True:
        stride=max(1,dimension>>2)*8 if flags else dimension*4;rows=max(1,dimension>>2) if flags else dimension
        pixels=bytes(range(1,stride*rows+1));address=p.allocate(len(pixels));f.allocations[address]=len(pixels);p.mu.mem_write(address,pixels)
        layouts.append((dimension,rows,stride,address))
        if dimension==1 or mode.endswith('partial'):break
        dimension>>=1
    vector=p.allocate(len(layouts)*16);f.allocations[vector]=len(layouts)*16
    for i,rec in enumerate(layouts):p.mu.mem_write(vector+i*16,struct.pack('<4I',*rec))
    p.put_uint(texture+0x70,vector);p.put_uint(texture+0x74,vector+len(layouts)*16);p.put_uint(texture+0x78,vector+len(layouts)*16)
    result=f.call(0x42bf40,this=serializer+0x10,args=(f.stream,texture))&255
    print('NATIVE_TEXTURE_WRITE',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),'errors',f.errors,flush=True)
    check(result==1 and not f.errors,'actual whole DX data writer returns successfully')
    check(f.data.startswith(b'\x22\x00\x00\xe6\x04\x00\x00\x00'+struct.pack('<I',6 if policy==1 else 7)),'source none plus platform6/7 UInt32BeginEnd')
    check(0x42b9e0 in p.visits and bool(p.visits.get(0x42dd70))==(policy!=1),'native/cross writer dispatch follows original serialization policy')
    capture=[mode,f.data.hex()]
    f.call(p.uint(p.uint(texture)),this=texture,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'native destructor releases supplied mip records/pixel buffers and all allocations')
    print(f'PASS {checks}/{checks}: original native TextureData writer {mode}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
