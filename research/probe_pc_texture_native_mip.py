#!/usr/bin/env python3
"""Tiny native DXData mip→actual DXTexture copy, explicit COM byte storage.

One1x1 level means no conversion helper or missing-mip synthesis is needed.
This executes original42C640/42C3B0/4ABBA0 to completion, not a GPU test.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_texture_fixtures import TextureUploadBoundaryFixture
from pc_texture_mip_fixtures import TextureMipChainFixture
from pc_loader_fixtures import empty_manager
from probe_pc_node_serializer import field

def main(mode,return_capture=False):
    parts=mode.split('-');kind=parts[0];size=int(parts[1]) if len(parts)>1 else 1
    if kind not in {'raw','dxt1','dxt3','dxt5'} or size not in (1,2,4):raise ValueError('four tiny native mip formats/full chains')
    flags,row_bytes,dxformat={'raw':(0,4,0x15),'dxt1':(1,8,0x31545844),
        'dxt3':(2,16,0x33545844),'dxt5':(3,16,0x35545844)}[kind]
    checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    native=b'';layouts=[];dimension=size
    while True:
        stride=max(1,dimension>>2)*row_bytes if flags else dimension*row_bytes
        rows=max(1,dimension>>2) if flags else dimension;pixels=bytes(range(1,stride*rows+1))
        raw=struct.pack('<3I',dimension,stride,rows)+pixels
        if not layouts:raw=b'\1'+struct.pack('<3I',size,size,flags)+b'\1'+raw
        native+=field(1 if layouts else 0,raw);layouts.append((dimension,stride,rows,pixels))
        if dimension==1:break
        dimension>>=1
    native+=b'\0';data=field(2,b'\0')+b'\0'+field(6,struct.pack('<I',6))+field(1,native)+b'\0'
    if len(parts)>2:
        if parts[2]!='embedded':raise ValueError('explicit same-serializer source field3 fixture')
        data=field(3,data)+b'\0'
    f=(TextureMipChainFixture if size>1 else TextureUploadBoundaryFixture)(data);p=f.p
    f.pitch=row_bytes+4;p.mu.mem_write(f.pixel_storage,b'\xa5'*256)
    manager,_=empty_manager(f);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    obj=f.call(0x4ab520);serializer=f.call(0x42b660)
    result=f.call(0x42c640,this=serializer+0x10,args=(f.stream,obj))&255
    print('NATIVE_MIP_READ',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'errors',f.errors,'events',f.events,'state',[hex(p.uint(obj+i)) for i in (0x18,0x1c,0x20,0x24,0x28,0x2c,0x44,0x48)],flush=True)
    check(result==1 and f.position==len(data) and not f.errors,'complete actual source/local/native mip reader')
    check(f.events[0]==['CreateTexture',size,size,0,0,dxformat,1,0],'actual native mip path keeps exact dimensions and direct flags mapping')
    copies=[]
    for level,(_,stride,rows,pixels) in enumerate(layouts):
        address=f.levels[level]['pixels'] if size>1 else f.pixel_storage
        expected=b''.join(pixels[row*stride:(row+1)*stride]+b'\xa5'*4 for row in range(rows))
        copied=bytes(p.mu.mem_read(address,len(expected)));copies.append(copied.hex())
        check(copied==expected,'original rep-copy transfers each native row and leaves destination pitch padding')
    check(p.uint(obj+0x18)==len(layouts) and p.uint(obj+0x20)==flags and p.uint(obj+0x28)==size and p.uint(obj+0x2c)==size
        and bytes(p.mu.mem_read(obj+0x1c,1))==b'\1' and bytes(p.mu.mem_read(obj+0x24,1))==b'\1','actual runtime base fields copied from temporary native TextureData')
    check(p.uint(obj+0x44)==0xcccccccc and p.uint(obj+0x48)==0,'native4ABBA0 does NOT initialize runtime format44 or size48')
    check(not p.visits.get(0x60fdb4) and not p.visits.get(0x61039a),'no conversion/missing-mip helper executed or replaced')
    check(f.surface_refs==1 and not f.locked and f.texture_refs==1,'actual reader balances surface locks/refs')
    capture=[mode,data.hex(),copies[0] if size==1 else copies,[len(layouts),1,flags,1,size,size],f.events.copy()]
    f.call(0x4abb50,this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'actual temporary TextureData/mip/vector/storage and all native allocations freed')
    check(f.texture_refs==f.surface_refs==0 and f.device_refs==1,'original DX dtor balances final COM boundary references')
    if size>1:check(all(rec['refs']==0 and not rec['locked'] for rec in f.levels),'all native mip surfaces released')
    print('NATIVE_MIP_CAPTURE',capture,flush=True);print(f'PASS {checks}/{checks}: actual PC native mip {mode}; no conversion or GPU')
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
