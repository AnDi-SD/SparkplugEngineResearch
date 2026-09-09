#!/usr/bin/env python3
"""Actual MeshNavigationSet::SetMesh on a real tiny MeshBV resource."""
from pathlib import Path
import sys,struct,json
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_navigation_readers import initialize,cleanup
from probe_pc_node_serializer import field
def main():
    f=PCWriteBytesFixture();p=f.p;initialize(f)
    node=f.call(0x41a700);mesh=f.call(0x47aff0);serializer=f.call(0x4383b0)
    geometry=struct.pack('<3I3H3I9f',2,1,0,0,1,2,0,3,0,0,0,0,2,0,0,0,4,0)
    wire=field(0,geometry)+b'\0';f.data=wire;f.position=0
    assert f.call(0x438490,this=serializer+0x10,args=(f.stream,mesh))&255 and f.position==len(wire)
    p.put_uint(mesh+8,1) # independent external owner alongside embedded CollisionInfo
    states=[]
    for position,scale in [((0.,0.,0.),(1.,1.,1.)),((1.,2.,3.),(2.,-3.,4.))]:
        p.put_floats(node+0x20,position);p.put_floats(node+0x30,scale);p.put_uint(node+0xb0,p.uint(node+0xb0)|1)
        f.call(0x421420,this=node,args=(0,));f.call(0x444010,this=node,args=(mesh,))
        states.append({'bounds':bytes(p.mu.mem_read(node+0x174,24)).hex(),'sphere':bytes(p.mu.mem_read(node+0x18c,16)).hex(),
            'mesh_borrowed':p.uint(node+0xe4)==mesh,'embedded_primitive':p.uint(node+0xfc)==mesh,'references':p.uint(mesh+8)&65535})
    print('NAV_MESH_CAPTURE',json.dumps({'mesh_wire':wire.hex(),'states':states,'arena':p.allocated}),flush=True)
    cleanup(f,[node,mesh,serializer]);print('PASS actual navigation MeshBV binding',flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
