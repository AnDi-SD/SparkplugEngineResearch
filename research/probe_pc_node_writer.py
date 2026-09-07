#!/usr/bin/env python3
"""Original Node field writer and native reread; explicit bounded stream only."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager
from inspect_pc_san_keys import fields
from probe_pc_node_serializer import check
import probe_pc_node_serializer as counters

def main(mode,return_capture=False):
    if mode not in {'defaults','transforms','flags-off','threshold','fail-first'}:
        raise ValueError('explicit bounded Node writer mode required')
    f=PCWriteBytesFixture();p=f.p;f.call(0x6d38e0)
    manager,_=empty_manager(f);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    node=f.call(0x421e20);serializer=f.call(0x4638f0)
    if mode=='transforms':
        p.put_floats(node+0x20,(1.,2.,3.));p.put_floats(node+0x30,(2.,3.,4.))
        p.put_floats(node+0x40,(0.,1.,0.,-1.,0.,0.,0.,0.,1.));p.put_uint(node+0xb0,p.uint(node+0xb0)|0x1400)
    if mode=='flags-off':p.put_uint(node+0xb0,p.uint(node+0xb0)&~0x1c00)
    if mode=='threshold':p.put_uint(node+0x20,0x3a83126f);p.put_uint(node+0x24,0x3a831270)
    if mode=='fail-first':f.fail_write_call=1
    before=bytes(p.mu.mem_read(node,0xb4))
    result=f.call(0x463f10,this=serializer+0x10,args=(f.stream,node))&255;output=f.data
    decoded=[] if mode=='fail-first' else list(fields(output,0))
    print('NODE WRITER',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,
          'bytes',len(output),'fields',[(k,len(v)) for k,v,_ in decoded],'hex',output.hex(),'errors',f.errors,flush=True)
    check(result==int(mode!='fail-first'),'original scalar Node writer result')
    check(bytes(p.mu.mem_read(node,0xb4))==before,'writer does not change Node object storage')
    if mode=='fail-first':check(f.write_calls==1 and f.errors,'injected first stream failure propagates with diagnostic')
    else:
        check(not f.errors and f.position==len(output),'writer restores output cursor after fields')
        expected={'defaults':[8],'flags-off':[8],'threshold':[0,8],'transforms':[0,1,2,3,4,8]}[mode]
        check([k for k,_,_ in decoded]==expected,'native default suppression and field order')
        if mode in {'defaults','flags-off'}:check(output==bytes([0x28,int(mode=='defaults'),0]),'mandatory Animated field emits exact bool even false')
        if mode=='threshold':check(decoded[0][1]==struct.pack('<III',0x3a83126f,0x3a831270,0),'above-threshold component emits full position vector')
        reloaded=f.call(0x421e20);f.position=0
        check(f.call(0x463a70,this=serializer+0x10,args=(f.stream,reloaded))&255==1,'original reader accepts original writer output')
        check(f.position==len(output),'reread consumes full output')
        if mode=='flags-off':check(p.uint(reloaded+0xb0)&0x800 and not p.uint(node+0xb0)&0x800,'native false Animated does not roundtrip into a fresh default-true Node')
        if mode in {'transforms','threshold'}:check(p.floats(reloaded+0x20,3)==p.floats(node+0x20,3),'native reread preserves position')
        f.call(0x422220,this=reloaded,args=(1,))
    f.call(0x422220,this=node,args=(1,));f.call(0x4639b0,this=serializer,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native Node/writer/helper objects released')
    print(f'PASS {counters.checks}/{counters.checks}: original Node writer {mode}; no relationship indexing/whole save claim')
    return [mode,output.hex()] if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
