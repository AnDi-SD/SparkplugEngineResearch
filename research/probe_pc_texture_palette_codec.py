#!/usr/bin/env python3
"""Actual flat runtime texture codec + actual renderer palette registration.

Tiny1x1 surfaces, 1024-byte explicit palette, original reader/writer. No GPU.
Native texture destructor's surviving palette is recorded before external cleanup.
"""
from pathlib import Path
import hashlib,struct,sys
from pc_instruction_emulator import run_bounded
from pc_palette_fixtures import PaletteFixture

def main(mode,return_capture=False):
    if mode not in ('raw','indexed','indexed-upload-fail'):raise ValueError('explicit tiny palette cases')
    fmt=3 if mode=='raw' else 5;pixels=b'\x01\x02\x03\x04' if fmt==3 else b'\x9a';palette_bytes=bytes(range(256))*4
    data=struct.pack('<3I',1,1,fmt)+b'\x01'+palette_bytes+struct.pack('<I',1)+pixels
    f=PaletteFixture(data);p=f.p;f.pitch=len(pixels)+4
    p.mu.mem_write(f.pixel_storage,b'\xa5'*256)
    if mode.endswith('-fail'):f.palette_result=0x8876086c
    texture=f.call(0x4ab520);serializer=f.call(0x4b24c0);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    result=f.call(0x4b2950,this=serializer+0x10,args=(f.stream,texture))&255
    palette=p.uint(texture+0x40)
    print('PALETTE_CODEC_READ',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,flush=True)
    check(result==1 and f.position==len(data) and not f.errors,'original complete runtime read including palette')
    check(f.allocations.get(palette)==0x414 and p.uint(palette+0x10)==0,'original palette allocation and actual renderer registration')
    check(bytes(p.mu.mem_read(palette+0x14,1024))==palette_bytes,'original exact1024 palette bytes')
    check(f.palette_events==[[0,palette_bytes.hex()]],'actual original renderer uploads palette even when COM reports failure')
    check(bytes(p.mu.mem_read(f.pixel_storage,len(pixels)+4))==pixels+b'\xa5'*4,'original image row and untouched COM padding')
    check(f.texture_refs==f.surface_refs==1 and not f.locked,'balanced native runtime attachment')
    state=[p.uint(texture+off) for off in (0x28,0x2c,0x44,0x48)]
    f.data=b'';f.position=0
    result=f.call(0x4b27c0,this=serializer+0x10,args=(f.stream,texture))&255
    check(result==1 and f.data==data and not f.errors,'writer emits exact present byte/palette/pixels')
    check(f.texture_refs==f.surface_refs==1 and not f.locked,'writer balances surface calls')
    capture=[mode,data.hex(),state,f.data.hex()]
    f.call(0x4abb50,this=texture,args=(1,))
    check(palette not in f.freed,'native DX destructor leaves loaded palette allocation alive')
    check(p.uint(f.renderer+0xca10)==1 and p.uint(f.renderer+0xca1c)==0,'native DX destructor does not return palette index to renderer')
    # Declared leak cleanup after observing native lifetime; not fabricated native teardown.
    f.call(p.uint(p.uint(palette)),this=palette,args=(1,))
    f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed) and f.device_refs==1 and f.texture_refs==f.surface_refs==0,'all original objects/COM refs released after labelled external palette cleanup')
    print('PALETTE_CODEC_CAPTURE',mode,'bytes',len(data),'input/output SHA256',hashlib.sha256(data).hexdigest(),hashlib.sha256(f.data).hexdigest(),flush=True)
    print(f'PASS {checks}/{checks}: original PC palette codec {mode}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
