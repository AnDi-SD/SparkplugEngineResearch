#!/usr/bin/env python3
"""Original PC Init to internal pixel-transfer ENTRY, or size query to completion.

60FDB4 has a real body in the executable; not mocked/claimed to be an IAT call.
Entry stop discards the suspended test stack, then tears down explicit owners.
This is NOT successful full texture upload or a live graphics test.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_texture_fixtures import TextureUploadBoundaryFixture

def main(mode):
    checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    f=TextureUploadBoundaryFixture();p=f.p;obj=f.call(0x4ab520);buffer=0
    if mode.startswith('upload-'):
        index=int(mode.split('-')[1])
        if index not in range(8):raise ValueError('five CPU formats or three compression flags')
        fmt=index if index<5 else 0;flags=0 if index<5 else index-4
        pixel_size=(4,4,1,2,2)[fmt];expected_format=(0x15,0x16,0x29,0x17,0x1a)[fmt] if not flags else (0x31545844,0x33545844,0x35545844)[flags-1]
        buffer=f.call(0x475df0)
        f.call(0x475f30,this=buffer,args=(1,1,1,0,fmt))
        pixels=p.uint(buffer+0x1c);p.mu.mem_write(pixels,bytes(range(1,pixel_size+1)))
        p.run(0x423250,this=obj,args=(buffer,1,flags,1),stop_at=0x60fdb4)
        check(p.reg('EIP')==0x60fdb4,'actual engine reaches real conversion entry without executing/mocking it')
        args=f.args(p,10);surface,dst_palette,dst_rect,src,src_fmt,pitch,src_palette,rect,filter_value,key=args
        print('UPLOAD_OBSERVED',mode,'args',[hex(x) for x in args],'events',f.events,'base',[p.uint(obj+i) for i in (0x28,0x2c,0x44)],'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
        check(args[:4]==[f.surface,0,0,pixels] and src_fmt==(0x15,0x16,0x29,0x17,0x1a)[fmt]
            and pitch==pixel_size and src_palette==0 and filter_value==0xffffffff and key==0,'actual transfer arguments')
        check(list(struct.unpack('<4I',p.mu.mem_read(rect,16)))==[0,0,1,1],'source rectangle is raw buffer dimensions')
        check(f.events==[['CreateTexture',2,2,0,0,expected_format,1,0],['GetSurfaceLevel',0]],'actual creation uses normalized2x2, source remains1x1')
        check(p.uint(obj+0x44)==(fmt+3 if not flags else flags-1) and p.uint(obj+0x3c)==f.texture
            and p.uint(obj+0x28)==2 and p.uint(obj+0x2c)==2,'engine DX/base fields before conversion')
        print('UPLOAD_BOUNDARY',mode,'args',[hex(x) for x in args],'events',f.events,'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
        # Explicit stop destroys the suspended emulator stack, not a completed
        # engine return. Release the one outstanding test-scope surface owner.
        p.put_uint(f.teb,0xffffffff)
        f.call(0x34070080,args=(f.surface,))
    elif mode.startswith('size-'):
        cases={'size-raw':(0x15,2,3,12,36),'size-dxt1':(0x31545844,1,1,0,8),
            'size-dxt3':(0x33545844,8,4,0,32),'size-dxt5-floor':(0x35545844,6,5,0,16)}
        fmt,width,height,pitch,expected=cases[mode]
        f.format=fmt;f.width=width;f.height=height;f.pitch=pitch;f.texture_refs=1;f.surface_refs=1;p.put_uint(obj+0x3c,f.texture)
        p.put_uint(obj+0x48,0x12345678)
        for iteration in (1,2):
            result=f.call(0x4aac50,this=obj)&255
            print('SIZE_OBSERVED',iteration,result,p.uint(obj+0x48),'slot',hex(p.uint(0x13b148c)),'events',f.events,'instructions',sum(p.visits.values()),flush=True)
            check(result==1 and p.uint(obj+0x48)==expected,'native protected prefix RESETS accumulator before adding surfaces')
            check(f.surface_refs==1 and not f.locked,'native size balances surface and lock')
        check(any(row[0]=='LockRect' for row in f.events)==(mode=='size-raw'),'compressed size never locks pixels')
        print('TEXTURE_SIZE',mode,'bytes',p.uint(obj+0x48),'events',f.events,'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    else:raise ValueError('explicit bounded boundary/size mode')
    f.call(0x4abb50,this=obj,args=(1,))
    if buffer:f.call(p.uint(p.uint(buffer)),this=buffer,args=(1,))
    for address in (0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(f.texture_refs==f.surface_refs==0 and f.device_refs==1,'original DX dtor balances declared texture/device references')
    check(set(f.allocations)==set(f.freed),'all actual native allocations freed after explicit test teardown')
    print(f'PASS {checks}/{checks}: actual PC texture {mode}; no conversion body or GPU execution')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
