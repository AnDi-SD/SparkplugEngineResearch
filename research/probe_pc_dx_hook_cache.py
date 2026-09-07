#!/usr/bin/env python3
"""PC DX hook cache-only paths: no vertex upload, combiner or GPU boundary."""
from pathlib import Path
import sys
import probe_pc_read_reference as refs
from probe_pc_read_reference import ReadReferenceFixture,check
from pc_instruction_emulator import run_bounded


def main(mode):
    if mode not in ('mesh-data','mesh-base','common','bit4','common-pc','pc-bit4'):raise ValueError('bounded cache-only mode required')
    f=ReadReferenceFixture(b'');p=f.p
    expected=0x3f077b6c if mode=='mesh-base' else 0x33c34cf0
    mask={'common':1,'bit4':4,'common-pc':3,'pc-bit4':6}.get(mode,2)
    p.put_uint(f.manager+0x10,mask)
    rtti=p.uint(0x755378);node=p.uint(p.uint(rtti+0x18)+4)
    p.put_uint(node+12,0x33c34cf0);p.put_uint(node+16,0x75d428)
    for record,identity,parent in ((0x75d428,0x33c34cf0,0x75e090),
            (0x75e090,0x3f077b6c,0x7603a0),(0x7603a0,0x46f043fe,0x7555f8),
            (0x7555f8,0x44de07fd,0x755310)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    mesh=f.call(0x41a270);f.objects.append(mesh)
    p.put_uint(mesh+0x50,0);p.put_uint(mesh+0x54,0) # explicit empty-resource input
    name=p.allocate(6);p.mu.mem_write(name,b'cache\0');f.call(0x4130f0,this=mesh,args=(name,))
    owned=p.allocate(6);p.mu.mem_write(owned,b'cache\0');f.allocations[owned]=6
    p.put_uint(f.entry+0xc,owned);p.put_uint(f.entry+0x10,expected)
    resources=f.call(0x458d00);f.call(0x458cb0,this=resources,args=(mesh,))
    # Registration classifies actual MeshData; keep it registered until that
    # call finishes, then supply the separate expected-base lookup fixture.
    if mode=='mesh-base':p.put_uint(node+12,expected);p.put_uint(node+16,0x75e090)
    # A base-class category lookup can find this MeshData even though DX hook
    # must only pre-materialize exact serialized MeshData IDs.
    check(f.call(0x4586b0,this=resources,args=(expected,owned))==mesh,'native cache category accepts selected entry class')
    hook=f.call(0x4aa430);f.call(p.uint(p.uint(hook)+0x1c),this=hook,args=(f.fat,f.stream))
    visited=set(p.visits)
    active=bool(mask&2) and mode!='mesh-base'
    check(p.uint(f.entry+0x20)==(mesh if active else 0),'DX hook touches exact MeshData only when PC bit2 enabled')
    check((0x4586b0 in visited)==active,'original cache lookup occurs only for active exact ID')
    check((0x4aa870 in visited)==bool(mask&2),'hook slot gate requires bit2; bit4 alone never enables it')
    check(0x4aa4e0 not in visited and 0x4a9610 not in visited and not f.io,'no metadata read/combiner/GPU path needed on cached or excluded entry')
    check(p.uint(0x763148)==0,'no temporary combiner published')
    f.call(p.uint(p.uint(hook)),this=hook,args=(1,));f.cleanup()
    print(f'PASS {refs.checks}/{refs.checks}: original PC DX hook {mode} cache selection')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
