#!/usr/bin/env python3
"""Actual PC Fog factory/field codec on bounded scalars and unchanged SMO slice."""
from pathlib import Path
import hashlib,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_serializer import field

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

def fixture_data(mode):
    normal=struct.pack('<IIfff',3,0x12345678,-2,123,.25)
    if mode=='empty':return b'\0'
    if mode=='values':return field(0,normal)+b'\0'
    if mode=='repeat':return field(0,bytes(20))+field(9,b'X')+field(0,normal)+b'\0'
    if mode=='raw-bits':return field(0,struct.pack('<5I',0xffffffff,0x1020304,0x80000000,0x7fc12345,0x7f800000))+b'\0'
    if mode=='failed-first-word':return bytes([0xa0,20])
    if mode=='failed-end':return bytes([0xa0,20])+normal[:12]
    if mode=='fail-write':return field(0,normal)+b'\0'
    if mode=='logo-field':
        raw=(ROOT/'local-data/pc-pristine/Media/Menus/logo_screen.smo').read_bytes()
        check(hashlib.sha256(raw).hexdigest().upper()=='DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7','unchanged703byte SMO hash')
        check(raw[416:424]==struct.pack('<II',0x7ac95aec,0x4f4f4253),'FAT ID5 Fog header at origin181+offset235')
        return raw[424:447]
    raise ValueError('explicit bounded Fog case')

def main(mode,return_capture=False):
    data=fixture_data(mode);f=PCWriteBytesFixture(data);p=f.p
    fog=f.call(0x419e90);serializer=f.call(0x43b830)
    check(f.allocations[fog]==0x28 and f.allocations[serializer]==0x14,'actual Fog28 and serializer14 allocations')
    before=bytes(p.mu.mem_read(fog+0x14,20))
    result=f.call(0x43b910,this=serializer+0x10,args=(f.stream,fog))&255
    state=bytes(p.mu.mem_read(fog+0x14,20))
    print('FOG READ',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,'position',f.position,'state',state.hex(),'errors',f.errors,flush=True)
    check(result==int(not mode.startswith('failed-')),'actual read return')
    check(f.position==len(data),'read consumes supplied tiny data')
    if mode=='failed-first-word':check(state==before and len(f.errors)==1,'first missing word leaves payload unchanged')
    elif mode=='failed-end':check(state[:12]==data[2:14] and state[12:]==before[12:] and len(f.errors)==1,'native earlier type/color/start mutations survive later failure')
    else:check(not f.errors,'valid Fog bytes have no diagnostic')
    captured=[mode,data.hex(),result,f.position,state.hex(),None]
    if result:
        f.data=b'';f.position=0
        if mode=='fail-write':f.fail_write_call=1
        written=f.call(0x43bcf0,this=serializer+0x10,args=(f.stream,fog))&255
        print('FOG WRITE',mode,'result',written,'instructions',sum(p.visits.values()),'hex',f.data.hex(),'errors',f.errors,flush=True)
        check(written==int(mode!='fail-write'),'original writer return')
        check(bytes(p.mu.mem_read(fog+0x14,20))==state,'writer does not alter even nonfinite/raw scalar bits')
        if written:check(f.data==field(0,state)+b'\0','exact UInt8-length field20bytes+terminator')
        else:check(not f.data and bool(f.errors),'first write failure no output')
        captured[-1]=f.data.hex() if written else None
    f.call(p.uint(p.uint(fog)),this=fog,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    for address in (0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all actual scalar native allocations released')
    print('FOG_CAPTURE',captured,flush=True);print(f'PASS {checks}/{checks}: original Fog {mode}')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
