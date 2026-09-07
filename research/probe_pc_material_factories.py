#!/usr/bin/env python3
"""Bounded original PC material factories/destructors; no renderer or PS2 layout."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

ENTRIES={'material-data':0x41a390,'material-serializer':0x477f00,
    'data-serializer':0x42f690,'dx-data-serializer':0x42f3e0,
    'pass':0x45f610,'texture-layer':0x423470,'std-layer':0x460e50,
    'dx-material':0x4a9460,'dx-serializer':0x4b0dd0}

def main(mode):
    checks=0
    def check(value,label):
        nonlocal checks
        checks+=1
        if not value:raise AssertionError(label)
    f=PCFileBytesFixture(b'');p=f.p;obj=f.call(ENTRIES[mode])
    check(obj in f.allocations,'factory result must be a tracked native allocation')
    size=f.allocations[obj]
    check(size=={'material-data':0xbc,'material-serializer':0x3c,'data-serializer':0x3c,'dx-data-serializer':0x3c,'pass':0x38,'texture-layer':0x14,'std-layer':0x14,'dx-material':0xbc,'dx-serializer':0x3c}[mode],'exact original allocation, not rounded PS2 extent')
    print('MATERIAL FACTORY',mode,'size',hex(size),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    for offset in range(0,size,4):print(f'+{offset:02X}: {p.uint(obj+offset):08X}')
    vt=p.uint(obj);print('PRIMARY',[hex(p.uint(vt+i)) for i in range(0,28,4)],flush=True)
    if 'serializer' in mode:
        secondary=p.uint(obj+0x10);print('SECONDARY',[hex(p.uint(secondary+i)) for i in range(0,12,4)],flush=True)
        check(secondary==(0x6efd68 if mode in {'data-serializer','dx-serializer'} else 0x6de514),'actual secondary table')
    if mode=='material-data':
        secondary=p.uint(obj+0x14);print('MATERIAL INTERFACE',[hex(p.uint(secondary+i)) for i in range(0,44,4)],flush=True)
        check(secondary==0x6de9d0,'material adjusted-this interface table, not neighboring Node table')
    if mode=='std-layer':
        nested=p.uint(obj+0x10)
        print('STD NESTED',hex(nested),'size',f.allocations.get(nested),'vtable',hex(p.uint(nested)),flush=True)
        check(f.allocations.get(nested)==0x68,'StdLayer owns exact PC MaterialTexture68, not old guessed6C')
    f.call(p.uint(vt),this=obj,args=(1,))
    for address in (0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    remaining=[(hex(a),s,hex(p.uint(a))) for a,s in f.allocations.items() if a not in f.freed]
    check(not remaining,f'unreleased native allocations {remaining}')
    print(f'PASS {checks}/{checks}: actual material factory/dtor; external static defaults not reinitialized')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
