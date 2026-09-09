#!/usr/bin/env python3
"""Actual spQuad factory/destructor with an explicit externally owned material."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_navigation_readers import initialize,cleanup

def main():
    f=PCWriteBytesFixture();p=f.p;initialize(f)
    quad=f.call(0x4cd7f0);material=f.call(0x4a9460)
    assert f.allocations[quad]==0x48 and p.uint(quad+0x10)==0 and p.uint(quad+0x14)==0
    # Prepared ownership state, not a setter execution claim: external ref1 + Quad ref1.
    p.put_uint(material+8,(p.uint(material+8)&0xffff0000)|2);p.put_uint(quad+0x14,material)
    f.call(p.uint(p.uint(quad)),this=quad,args=(1,))
    report={'size':0x48,'quad_freed':quad in f.freed,'material_refs':p.uint(material+8)&65535,'material_alive':material not in f.freed}
    assert report['quad_freed'] and report['material_refs']==1 and report['material_alive']
    cleanup(f,[material]);report['arena_bytes']=p.allocated;report['all_native_allocations_freed']=set(f.allocations)==set(f.freed)
    print('QUAD_OWNERSHIP_CAPTURE',json.dumps(report),flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
