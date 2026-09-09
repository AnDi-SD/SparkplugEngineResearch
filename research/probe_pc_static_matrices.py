#!/usr/bin/env python3
"""Bounded original StaticRenderObject constructor/serializer matrix scout."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture

def main():
    f=PCWriteBytesFixture();p=f.p
    # Explicit zero renderer cache storage for support destruction, inside the
    # normal micro arena; no renderer constructor, vtable or device calls.
    renderer=p.allocate(0xca00);p.put_uint(0x75db68,renderer)
    f.call(0x6d38c0)
    obj=f.call(0x41a7c0);serializer=f.call(0x44fce0)
    report={'object_size':f.allocations[obj],'serializer_size':f.allocations[serializer],
        'object_vtable':hex(p.uint(obj)),'serializer_vtable':hex(p.uint(serializer)),
        'secondary_vtable':hex(p.uint(serializer+0x10)),
        # The secondary interface has three slots; the following words belong
        # to the adjacent primary vtable and must not be reported as its slots.
        'secondary_slots':[hex(p.uint(p.uint(serializer+0x10)+i*4)) for i in range(3)],
        'world':p.floats(obj+0x8c,16),'inverse':p.floats(obj+0xcc,16)}
    for value in (obj,serializer):f.call(p.uint(p.uint(value)),this=value,args=(1,))
    assert set(f.allocations)==set(f.freed),'all original owners released'
    report['released']=len(f.freed);print(json.dumps(report));return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
