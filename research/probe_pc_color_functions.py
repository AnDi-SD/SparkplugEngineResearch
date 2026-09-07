#!/usr/bin/env python3
"""Bounded PC ColorFuncEval/material color scout; actual finite scalar code."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup,configure_floor,bits
from probe_pc_node_serializer import field

def main(mode,return_capture=False):
    if mode=='material-factory':raise ValueError('Disabled: original41A580 hit100k/time cap at88CD04 in CP23 scout; never resumed or retried')
    modes=['factory','clone','blend','blend-white','blend-default','codec-default','codec-values','codec-repeat','codec-zero','codec-nan']+[f'type{i}' for i in range(10)]
    if mode not in modes:raise ValueError('explicit small color case')
    f=PCWriteBytesFixture();p=f.p;configure_floor(f);animations=f.call(0x454640);checks=0;objects=[];states=[];payload=b'';written=b'';maximum=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    obj=f.call(0x478a60)
    check(f.allocations[obj]==0x50 and p.uint(obj)==0x6de684 and p.uint(obj+0x18)==0x6ea9ac,'actual ColorFunc50 and embedded Function38')
    check(p.uint(obj+0x10)==p.uint(obj+0x14)==0xff000000,'PC endpoints default opaque black')
    if mode=='clone':
        f.call(0x52fd90,this=0x755588);p.put_uint(obj+0x10,0x12345678);p.put_floats(obj+0x18+0x24,(7.,));p.put_uint(obj+0x4c,8)
        clone=f.call(0x478ac0,this=obj);objects.append(clone)
        check(p.uint(clone+0x10)==p.uint(clone+0x14)==0xff000000 and p.uint(clone+0x4c)==0 and p.floats(clone+0x18+0x24,1)==(0.,),'standalone virtual clone keeps defaults')
        states=[p.uint(clone+0x10),p.uint(clone+0x14),p.uint(clone+0x4c),p.floats(clone+0x18+0x24,1)[0]];f.call(0x6d7db0)
    elif mode.startswith('blend'):
        if mode=='blend':p.put_uint(obj+0x10,0x80402010);p.put_uint(obj+0x14,0xff112244)
        if mode=='blend-white':p.put_uint(obj+0x10,0xffffffff);p.put_uint(obj+0x14,0xffffffff)
        output=p.allocate(4)
        for value in (-3.,-1.,-.5,0.,.5,1.,3.):
            p.put_floats(obj+0x18+0x24,(value,));f.call(0x478990,this=obj,args=(output,bits(.25)))
            states.append([value,p.uint(output)]);maximum=max(maximum,sum(p.visits.values()))
            check(p.floats(obj+0x28,1)==(0.,),'constant color does not advance embedded clock')
    elif mode.startswith('type'):
        p.put_uint(obj+0x10,0x80402010);p.put_uint(obj+0x14,0xff112244);p.put_uint(obj+0x4c,int(mode[4:]));p.put_floats(obj+0x2c,(2.,.5,1.5,.125,-.25,.5))
        output=p.allocate(4)
        for delta in (.25,.5,.75,-.5,1.):
            f.call(0x478990,this=obj,args=(output,bits(delta)));states.append([delta,p.uint(output),p.floats(obj+0x28,1)[0]])
            maximum=max(maximum,sum(p.visits.values()));check(p.reg('EAX')==output,'color return points to caller output')
    elif mode.startswith('codec'):
        if mode=='codec-values':payload=b''.join(field(i,struct.pack('<I',v)) for i,v in enumerate((0x80402010,0xff112244,8,bits(2),bits(1.5),bits(.125),bits(-.25),bits(.5))))
        if mode=='codec-repeat':payload=field(15,b'ignore')+field(0,struct.pack('<I',0x12345678))+field(0,struct.pack('<I',0x80402010))+field(3,struct.pack('<f',-.5))
        if mode=='codec-zero':payload=field(3,struct.pack('<I',0))+field(5,struct.pack('<I',0x80000000))
        if mode=='codec-nan':payload=field(4,struct.pack('<I',0x7fc12345))+field(6,struct.pack('<I',0x7fc54321))
        payload+=b'\0';f.data=payload;f.position=0;serializer=f.call(0x47e240);objects.append(serializer)
        result=f.call(0x47e320,this=serializer+0x10,args=(f.stream,obj))&255
        check(result==1 and f.position==len(payload) and not f.errors,'actual color common-core reader')
        f.data=b'';f.position=0;result=f.call(0x47e850,this=serializer+0x10,args=(f.stream,obj))&255
        check(result==1 and not f.errors,'actual color common-core writer');written=f.data
    for owner in objects:f.call(p.uint(p.uint(owner)),this=owner,args=(1,))
    f.call(p.uint(p.uint(obj)),this=obj,args=(1,));f.call(0x4545d0,this=animations,args=(1,));cleanup(f,())
    check(set(f.allocations)==set(f.freed),'all actual tracked allocations released')
    capture=[mode,payload.hex(),states,written.hex()];print('COLOR_FUNCTION_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original ColorFunc {mode}; heap={p.allocated}; maxInstructions={maximum}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
