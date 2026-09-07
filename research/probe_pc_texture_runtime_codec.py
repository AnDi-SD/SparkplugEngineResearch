#!/usr/bin/env python3
"""Original runtime DXTextureSerializer flat codec with1x1 declared COM storage."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_texture_fixtures import TextureUploadBoundaryFixture
from pc_texture_mip_fixtures import TextureMipChainFixture

def main(mode,return_capture=False):
    parts=str(mode).split('-');fmt=int(parts[0]);size=int(parts[1]) if len(parts)>1 else 1
    if fmt not in range(8):raise ValueError('eight original runtime formats')
    if size not in (1,2,4):raise ValueError('tiny full-chain sizes')
    layouts=[];dimension=size
    while True:
        row_size=max(1,dimension>>2)*(8 if fmt==0 else 16) if fmt<3 else dimension*(4,4,1,2,2)[fmt-3]
        rows=max(1,dimension>>2) if fmt<3 else dimension
        pixels=bytes(range(1,row_size*rows+1));layouts.append((dimension,row_size,rows,pixels))
        if dimension==1:break
        dimension>>=1
    data=struct.pack('<3I',size,size,fmt)+b'\0'+struct.pack('<I',len(layouts))+b''.join(row[3] for row in layouts)
    f=(TextureMipChainFixture if size>1 else TextureUploadBoundaryFixture)(data);p=f.p
    f.pitch=layouts[0][1]+4;p.mu.mem_write(f.pixel_storage,b'\xa5'*256)
    obj=f.call(0x4ab520);serializer=f.call(0x4b24c0);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    result=f.call(0x4b2950,this=serializer+0x10,args=(f.stream,obj))&255
    state=[p.uint(obj+off) for off in (0x28,0x2c,0x44,0x48)]
    print('RUNTIME_TEX_READ',fmt,result,'state',state,'events',f.events,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'errors',f.errors,flush=True)
    check(result==1 and f.position==len(data) and not f.errors,'actual runtime flat texture read to completion')
    expected_bytes=sum((row_size if fmt<3 else row_size+4)*rows for _,row_size,rows,_ in layouts)
    check(state==[size,size,fmt,expected_bytes],'runtime reader sets format and actual size(pitch for uncompressed)')
    for level,(_,row_size,rows,pixels) in enumerate(layouts):
        address=f.levels[level]['pixels'] if size>1 else f.pixel_storage
        expected=b''.join(pixels[row*row_size:(row+1)*row_size]+b'\xa5'*4 for row in range(rows))
        check(bytes(p.mu.mem_read(address,len(expected)))==expected,'row payload copied, physical pitch padding untouched at each mip')
    check(f.texture_refs==f.surface_refs==1 and not f.locked,'reader attach AddRef balances temporary texture owner')
    check(p.uint(obj+0x18)==0xcccccccc and bytes(p.mu.mem_read(obj+0x1c,1))==b'\0','runtime attach leaves base field18/1C uninitialized/default unlike nativeData init')
    f.data=b'';f.position=0;f.events=[]
    result=f.call(0x4b27c0,this=serializer+0x10,args=(f.stream,obj))&255
    print('RUNTIME_TEX_WRITE',fmt,result,'hex',f.data.hex(),'events',f.events,'instructions',sum(p.visits.values()),'heap',p.allocated,'errors',f.errors,flush=True)
    check(result==1 and f.data==data and not f.errors,'actual runtime writer preserves tight flat payload, excludes COM pitch padding')
    check(f.texture_refs==f.surface_refs==1 and not f.locked,'native writer balances repeated GetSurface/locks')
    captured=[fmt if size==1 else str(mode),data.hex(),state,f.data.hex()]
    f.call(0x4abb50,this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    for address in (0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native objects freed')
    check(f.device_refs==1 and f.texture_refs==f.surface_refs==0,'native DX destructor balances COM storage')
    if size>1:check(all(rec['refs']==0 and not rec['locked'] for rec in f.levels),'all mip surfaces released')
    print('RUNTIME_TEX_CAPTURE',captured,flush=True);print(f'PASS {checks}/{checks}: PC runtime texture codec format{fmt};no GPU')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
