#!/usr/bin/env python3
"""Small actual PC texture factories/header/destructors; never initializes GPU."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture
from pc_texture_fixtures import TextureDeviceFixture

ENTRIES={'data':0x41a2d0,'dx':0x4ab520,'buffer':0x475df0,
    'data-serializer':0x42dc30,'dx-data-serializer':0x42b660,'dx-serializer':0x4b24c0,'header':0x42dd10}

def main(mode):
    checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    fixture=TextureDeviceFixture if mode in {'dx','header'} else PCFileBytesFixture
    f=fixture(struct.pack('<II',0x12345678,0xabcdef01));p=f.p
    obj=f.call(ENTRIES[mode],args=(f.stream,)) if mode=='header' else f.call(ENTRIES[mode])
    check(obj in f.allocations,'actual tracked allocation')
    size=f.allocations[obj];vt=p.uint(obj)
    print('TEXTURE_FACTORY',mode,hex(obj),'size',hex(size),'vtable',hex(vt),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    print('PRIMARY',[hex(p.uint(vt+i)) for i in range(0,44,4)],flush=True)
    for off in range(0,min(size,0x90),4):print(f'+{off:02X}: {p.uint(obj+off):08X}',flush=True)
    if 'serializer' in mode:
        check(size==0x14,'actual PC serializer extent14')
        secondary=p.uint(obj+0x10);print('SECONDARY',[hex(p.uint(secondary+i)) for i in range(0,12,4)],flush=True)
    if mode=='header':check(f.position==8 and not f.errors,'actual header consumes arbitrary eight bytes')
    f.call(p.uint(vt),this=obj,args=(1,))
    for address in (0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native texture factory allocations freed')
    if isinstance(f,TextureDeviceFixture):
        check(f.device_refs==1 and f.device_events==['AddRef','Release'],'original ctor/dtor balance one COM device reference; no GPU call')
    print(f'PASS {checks}/{checks}: actual PC texture {mode}; no texture upload or graphics API')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
