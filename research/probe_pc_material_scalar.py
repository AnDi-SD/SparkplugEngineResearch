#!/usr/bin/env python3
"""Original PC common MaterialData scalar codec; no guessed renderer behavior."""
from pathlib import Path
import hashlib,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_serializer import field

def fixture_data(mode):
    values=field(0,struct.pack('<11I',9,8,7,6,5,4,3,2,1,0,0xffffffff))+field(1,b'\x02')+field(2,struct.pack('<4If',0x12345678,0xabcdef01,0xff102030,0x87654321,3.5))
    if mode=='empty':return b'\0'
    if mode=='values':return values+b'\0'
    if mode=='repeat':return field(1,b'\x01')+field(18,b'ignored')+values+field(1,b'\0')+b'\0'
    if mode=='null-controller':return field(6,struct.pack('<I',0))+b'\0'
    if mode=='pass-only':return field(3,struct.pack('<I',2))+b'\0'
    if mode=='logo-field':
        raw=(ROOT/'local-data/pc-pristine/Media/Menus/logo_screen.smo').read_bytes()
        if hashlib.sha256(raw).hexdigest().upper()!='DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7':raise AssertionError('unchanged logo SMO')
        if raw[269:277]!=struct.pack('<II',0x6160348b,0x4f4f4253):raise AssertionError('FAT ID4 actual MaterialData header')
        return raw[277:403]
    raise ValueError('explicit bounded Material scalar mode')

def main(mode,return_capture=False):
    if mode=='logo-field':
        raise ValueError('DISABLED: original full logo material exceeded32KiB allocation guard with incomplete layer/RTTI startup; no retry or raised cap. Partial scalar fields are not whole-material proof.')
    checks=0
    def check(value,label):
        nonlocal checks
        checks+=1
        if not value:raise AssertionError(label)
    data=fixture_data(mode);f=PCWriteBytesFixture(data);p=f.p
    material=f.call(0x41a390);serializer=f.call(0x42f690)
    check(f.allocations[material]==0xbc and f.allocations[serializer]==0x3c,'actual MaterialData BC/serializer3C factories')
    secondary=p.uint(serializer+0x10)
    result=f.call(p.uint(secondary+8),this=serializer+0x10,args=(f.stream,material))&255
    state=(bytes(p.mu.mem_read(material+0x18,44))+bytes(p.mu.mem_read(material+0x6d,1))+bytes(p.mu.mem_read(material+0x78,68))).hex()
    pass_count=p.uint(material+0x48)
    print('MATERIAL READ',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,'cursor',f.position,'state',state,'passes',pass_count,'errors',f.errors,flush=True)
    check(result==1 and f.position==len(data) and not f.errors,'actual entire material scalar section read')
    captured=[mode,data.hex(),result,f.position,state,pass_count,None]
    f.data=b'';f.position=0
    written=f.call(p.uint(secondary),this=serializer+0x10,args=(f.stream,material))&255
    print('MATERIAL WRITE',mode,'result',written,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),'errors',f.errors,flush=True)
    check(written==1 and not f.errors,'actual complete writer secondary dispatch')
    captured[-1]=f.data.hex()
    f.call(p.uint(p.uint(material)),this=material,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    for address in (0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all original material/embedded block/pass allocations released')
    print('MATERIAL_CAPTURE',captured,flush=True);print(f'PASS {checks}/{checks}: original material scalar {mode}')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
