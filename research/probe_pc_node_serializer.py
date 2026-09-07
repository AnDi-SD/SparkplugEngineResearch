#!/usr/bin/env python3
"""Bounded original Node field reader; no relationship/body substitutes."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

def field(kind,payload):
    if not 0<=kind<31 or not 0<len(payload)<256:raise ValueError('tiny valid field fixture only')
    return bytes([0xa0+kind,len(payload)])+payload

def main(mode,return_capture=False):
    if mode not in {'empty','transforms','false-flags','repeat-flags','unknown-repeat','null-target','failed-position'}:
        raise ValueError('explicit bounded Node reader case required')
    data=b'\0'
    if mode=='transforms':
        data=field(0,struct.pack('<3f',1,2,3))+field(1,struct.pack('<4f',0,0,.5,.5))
        data+=field(2,struct.pack('<3f',2,3,4))+field(3,b'\xa5')+field(4,b'\1')+field(8,b'\0')+b'\0'
    if mode=='false-flags':data=field(3,b'\0')+field(4,b'\0')+field(8,b'\0')+b'\0'
    if mode=='repeat-flags':data=b''.join(field(k,bytes([v])) for v in (1,0) for k in (3,4,8))+b'\0'
    if mode=='unknown-repeat':data=field(9,b'X')+field(0,struct.pack('<3f',1,2,3))+field(0,struct.pack('<3f',4,5,6))+b'\0'
    if mode=='failed-position':data=bytes([0xa0,12])
    f=PCFileBytesFixture(data);p=f.p
    f.call(0x6d38e0) # original small matrix identity startup, not guessed input
    node=f.call(0x421e20);serializer=f.call(0x4638f0)
    check(f.allocations[serializer]==0x14,'original NodeSerializer factory exact14, not only observed extent')
    check(f.allocations[node]==0xb4,'original Node allocation extent')
    result=f.call(0x463a70,this=serializer+0x10,args=(f.stream,0 if mode=='null-target' else node))&255
    print('NODE READER',mode,'result',result,'instructions',sum(p.visits.values()),'heap',p.allocated,
          'position',f.position,'flags',hex(p.uint(node+0xb0)),'local',p.floats(node+0x20,3),p.floats(node+0x40,9),
          'world',p.floats(node+0x74,3),p.floats(node+0x8c,9),'errors',f.errors,flush=True)
    check(result==int(mode not in {'null-target','failed-position'}),'reader result from original return path')
    if mode not in {'null-target','failed-position'}:
        check(f.position==len(data) and not f.errors,'complete fields and terminator read without diagnostic')
        check(0x420960 in p.visits or p.uint(node+0xb0)&1==0,'normal reader calls virtual world update after local mutation')
    if mode in {'false-flags','repeat-flags'}:
        check(bool(p.uint(node+0xb0)&0x800),'false Animated field does not clear constructor/default or previous true state')
        check(bool(p.uint(node+0xb0)&0x400)==(mode=='repeat-flags'),'false Static field leaves previous true state unchanged')
        check(not p.uint(node+0xb0)&0x1000,'false Bone field actively clears state unlike Static/Animated')
    if mode in {'transforms','unknown-repeat'}:
        expected=(1.,2.,3.) if mode=='transforms' else (4.,5.,6.)
        check(p.floats(node+0x20,3)==p.floats(node+0x74,3)==expected,'reader local changes propagated by actual final world update')
    if mode=='transforms':
        check(p.floats(node+0x30,3)==p.floats(node+0x80,3)==(2.,3.,4.),'scale reaches local and cached world state')
        check(p.floats(node+0x40,9)==p.floats(node+0x8c,9)==(.5,.5,0.,-.5,.5,0.,0.,0.,1.),'quaternion not normalized by actual conversion')
        check(p.uint(node+0xb0)&0x1c00==0x1c00,'nonzero byte Bone/Static and unchanged Animated masks')
    if mode=='null-target':check(f.position==0 and not f.errors,'null target rejects before consuming input')
    if mode=='failed-position':check(f.position==len(data) and len(f.errors)==1,'failed payload read reports diagnostic and false')
    state=b''.join(bytes(p.mu.mem_read(node+offset,count*4)) for offset,count in
                   ((0x20,3),(0x30,3),(0x40,9),(0x74,3),(0x80,3),(0x8c,9)))
    captured=[mode,data.hex(),f.position,p.uint(node+0xb0),state.hex()]
    f.call(0x422220,this=node,args=(1,));f.call(0x4639b0,this=serializer,args=(1,))
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'original node/serializer/helper storage all released')
    print(f'PASS {checks}/{checks}: PC Node reader {mode}; child/collision references not yet exercised')
    return captured if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
