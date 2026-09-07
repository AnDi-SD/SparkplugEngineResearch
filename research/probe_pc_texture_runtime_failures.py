#!/usr/bin/env python3
"""Bounded original runtime texture failure returns and surviving ownership.

Deliberate failed byte-stream/COM boundaries, not a replaced engine algorithm.
Every observed live lock/reference/leaked palette is explicitly cleaned only
AFTER recording native return behavior. No GPU or host exceptions are invoked.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_palette_fixtures import PaletteFixture

def main(mode):
    if mode not in ('read-palette','create-fail','read-pixels','write-palette','write-pixels'):raise ValueError('five bounded runtime failure cases')
    header=struct.pack('<3I',1,1,3)+b'\x01';palette_bytes=bytes(range(256))*4
    valid=header+palette_bytes+struct.pack('<I',1)+b'\x01\x02\x03\x04'
    data=header if mode=='read-palette' else valid[:-4] if mode=='read-pixels' else valid
    f=PaletteFixture(data);p=f.p;f.pitch=8;checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    if mode=='create-fail':f.create_result=0x8876086c
    texture=f.call(0x4ab520);serializer=f.call(0x4b24c0)
    result=f.call(0x4b2950,this=serializer+0x10,args=(f.stream,texture))&255
    palettes=[a for a,size in f.allocations.items() if size==0x414]
    check(len(palettes)==1 and palettes[0] not in f.freed,'one original allocated surviving palette')
    if mode.startswith('write'):
        check(result==1 and not f.errors,'complete original input before write-failure experiment')
        f.data=b'';f.position=0;f.write_calls=0;f.fail_write_call=5 if mode=='write-palette' else 7
        result=f.call(0x4b27c0,this=serializer+0x10,args=(f.stream,texture))&255
        check(result==(1 if mode=='write-pixels' else 0),'outer writer ignores false mip helper but checks palette write')
        check(bool(f.errors),'original failed writer emits diagnostic at declared output seam')
        check(len(f.data)==(13 if mode=='write-palette' else len(valid)-4),'original partial output extent')
        check(f.locked==(mode=='write-pixels') and f.surface_refs==(2 if mode=='write-pixels' else 1),'failed mip writer leaves acquired surface locked/retained')
    else:
        check(result==(1 if mode=='create-fail' else 0),'failed CreateTexture returns true; explicit stream failures false')
        check(p.uint(texture+0x40)==0 and p.uint(texture+0x3c)==0,'failed read never attaches palette/texture to target')
        check(bool(f.errors)==(mode!='create-fail'),'CreateTexture failure has no checked native diagnostic')
        check(f.locked==(mode=='read-pixels') and f.texture_refs==(1 if mode=='read-pixels' else 0),'failed pixel read leaves temporary COM texture and lock alive')
    print('RUNTIME_FAILURE',mode,'return',result,'errors',f.errors,'textureRefs',f.texture_refs,'surfaceRefs',f.surface_refs,'locked',f.locked,
        'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    # Cleanup is explicitly OUTSIDE the observed native reader/writer contract.
    if f.locked:f.call(0x340700b0,args=(f.surface,))
    if f.surface_refs>1:f.call(0x34070080,args=(f.surface,))
    if f.texture_refs and not p.uint(texture+0x3c):f.call(0x34070050,args=(f.texture,))
    f.call(0x4abb50,this=texture,args=(1,))
    check(palettes[0] not in f.freed,'original DX destruction still leaves allocated palette alive')
    f.call(p.uint(p.uint(palettes[0])),this=palettes[0],args=(1,))
    f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed) and f.texture_refs==f.surface_refs==0 and f.device_refs==1,'all native allocations and explicit COM refs cleaned after observation')
    print(f'PASS {checks}/{checks}: original runtime texture {mode}; known native hazards not host policy',flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
