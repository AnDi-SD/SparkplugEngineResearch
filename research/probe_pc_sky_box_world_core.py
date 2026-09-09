#!/usr/bin/env python3
"""Bounded actual SkyBox factory/world update, with actual parent ownership."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_relationships import node_rtti

def main():
    f=PCWriteBytesFixture();p=f.p;f.call(0x6d38e0);node_rtti(f)
    for record,identity,parent in ((0x75e150,0x603625d0,0x75dd88),(0x762e10,0x7a7124af,0x75e150)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    p.put_uint(0x75db68,p.allocate(0xc9c8))
    root=f.call(0x421e20);sky=f.call(0x49e4c0)
    f.call(0x421a60,this=root,args=(sky,))
    p.put_floats(root+0x20,(10.,20.,30.));p.put_floats(root+0x30,(2.,3.,4.))
    p.put_floats(root+0x40,(0.,1.,0.,-1.,0.,0.,0.,0.,1.))
    p.put_floats(sky+0x20,(1.,2.,3.));p.put_floats(sky+0x40,(1.,0.,0.,0.,0.,1.,0.,-1.,0.))
    p.put_uint(root+0xb0,p.uint(root+0xb0)|1);p.put_uint(sky+0xb0,p.uint(sky+0xb0)|1)
    f.call(0x421420,this=root,args=(0,));f.call(0x49e440,this=sky,args=(0,))
    states=[bytes(p.mu.mem_read(sky+0x74,60)).hex()]
    # Explicit clean-bit input proves the unconditional derived orientation copy.
    p.put_floats(sky+0x40,(1.,0.,0.,0.,1.,0.,0.,0.,1.));p.put_uint(sky+0xb0,p.uint(sky+0xb0)&~1)
    f.call(0x49e440,this=sky,args=(0,));states.append(bytes(p.mu.mem_read(sky+0x74,60)).hex())
    f.call(p.uint(p.uint(root)),this=root,args=(1,))
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    assert set(f.allocations)==set(f.freed),'actual hierarchy and globals freed'
    print('SKY_WORLD_CAPTURE',json.dumps({'states':states,'arena':p.allocated,'freed':len(f.freed)}),flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
