#!/usr/bin/env python3
"""Bounded original PC scalar FunctionEval, codec and RNG dependency.

Only finite MSVCR71!floor (IAT6D9370) is a declared math-library boundary.
Function dispatch, x87 arithmetic, random generation, factories and codecs
execute original instructions. No renderer/startup/OS or cap increases.
"""
from pathlib import Path
import json,math,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_animation_lifecycle import x87_value
from probe_pc_node_serializer import field

def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def bits(x):return struct.unpack('<I',struct.pack('<f',x))[0]

def configure_floor(f):
    p=f.p;calls=[]
    p.mu.mem_map(0x34080000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def floor(machine):
        value=struct.unpack('<d',machine.mu.mem_read(machine.reg('ESP')+4,8))[0]
        if not math.isfinite(value):raise AssertionError('only finite declared CRT floor contract')
        calls.append(value);machine.fixture_push_x87(float(math.floor(value)));machine.fixture_return()
    p.put_uint(0x6d9370,0x34080010);p.seams[0x34080010]=floor
    return calls

def cleanup(f,objects):
    p=f.p
    for obj in objects:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    for address in (0x74e060,0x75526c,0x755264):
        owner=p.uint(address)
        if owner:f.call(p.uint(p.uint(owner)),this=owner,args=(1,))
    if set(f.allocations)!=set(f.freed):
        raise AssertionError(('native scalar cleanup',[(hex(a),s) for a,s in f.allocations.items() if a not in f.freed]))

def main(mode,return_capture=False):
    f=PCWriteBytesFixture();p=f.p;floor_calls=configure_floor(f);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    obj=f.call(0x478840)
    check(f.allocations[obj]==0x38 and p.uint(obj)==0x6ea9ac,'exact original FunctionEval factory')
    defaults=bytes(p.mu.mem_read(obj+0x10,0x28)).hex()
    check(p.floats(obj+0x10,8)==(0.,1.,1.,1.,0.,0.,0.,0.) and p.uint(obj+0x34)==0 and p.uint(obj+0x30)==0xcccccc00,'scalar defaults and untouched padding')
    serializer=f.call(0x47ed20);states=[];input_bytes=b'\0';written=b''
    if mode=='factory':
        f.call(0x52fd90,this=0x755588) # verified original global clone-map constructor
        p.put_floats(obj+0x10,(2.,3.,4.,5.,6.,7.,8.,9.));p.put_uint(obj+0x34,5)
        # Virtual copy is inherited Base40ECE0: clone factory stays blank.
        clone=f.call(0x4788d0,this=obj)
        check(bytes(p.mu.mem_read(clone+0x10,0x28)).hex()==defaults,'virtual clone does not copy scalar evaluator fields')
        f.call(0x6d7db0)
        cleanup(f,(clone,serializer,obj))
    elif mode=='rng':
        check(p.uint(0x73fe8c)==625 and p.uint(0x73fe94)==0x9908b0df,'native default RNG marker/twist coefficient')
        values=[];maximum=0
        for i in range(630):
            p.run(0x4132b0,callee_pop=False);value=p.reg('EAX');maximum=max(maximum,sum(p.visits.values()))
            values.append(value)
        check(values[:5]==[3499211612,581869302,3890346734,3586334585,545404204],'seed5489 known MT sequence follows actual native seed/twist')
        check(p.uint(0x73fe8c)==6,'second twist after624 draws')
        print('FUNCTION_RNG_MAX_INSTRUCTIONS',maximum,flush=True);states=values
        cleanup(f,(serializer,obj))
    else:
        if mode.startswith('type-'):kind=int(mode.removeprefix('type-'));frequency=1.;amplitude=1.;xoff=0.;yoff=0.;pitch=2.
        elif mode in ('configured','clamp-up','clamp-down','clamp-zero','negative-frequency','codec-zero-frequency','codec-negative-zero','codec-nan','unknown-repeat'):
            kind=8 if mode.startswith('clamp-') else 5;frequency=-2. if mode=='negative-frequency' else 0. if mode=='codec-zero-frequency' else 2.;amplitude=2.;xoff=.125;yoff=3.;pitch=-2. if mode=='clamp-down' else 0. if mode=='clamp-zero' else 2.
        else:raise ValueError('explicit scalar factory/rng/type0..9/configured/clamp/codec case')
        if not 0<=kind<=9:raise ValueError('small dispatch type')
        input_bytes=field(0,struct.pack('<I',kind))+b''.join(field(i,struct.pack('<f',v)) for i,v in enumerate((frequency,amplitude,xoff,yoff,pitch),1))+b'\0'
        if mode=='codec-negative-zero':input_bytes=b''.join(field(i,struct.pack('<I',0x80000000)) for i in (3,4,5))+b'\0'
        if mode=='codec-nan':input_bytes=field(2,struct.pack('<I',0x7fc12345))+field(3,struct.pack('<I',0x7fc54321))+b'\0'
        if mode=='unknown-repeat':input_bytes=field(13,b'ignored')+input_bytes[:-1]+field(4,struct.pack('<f',7.))+b'\0'
        f.data=input_bytes;f.position=0
        check(f.call(0x47ee00,this=serializer+0x10,args=(f.stream,obj))&255==1 and f.position==len(input_bytes) and not f.errors,'original scalar field reader')
        if mode.startswith('clamp-'):
            p.put_floats(obj+0x2c,(1. if mode=='clamp-down' else 4.,));p.mu.mem_write(obj+0x30,b'\1')
        loaded=[p.uint(obj+i) for i in range(0x10,0x38,4)]
        if not mode.startswith('codec-') and mode!='unknown-repeat':
            for delta in (0.,.25,.25,.5,.25,1.,-.5,-2.,.125):
                f.call(0x478680,this=obj,args=(bits(delta),));value=f32(x87_value(p))
                states.append([delta,value,p.floats(obj+0x10,1)[0]])
                check(math.isfinite(value),'bounded scalar finite output')
            check(len(floor_calls)==(0 if kind in (0,7) else len(states)),'constant types do not advance time or call floor')
        f.data=b'';f.position=0
        check(f.call(0x47f220,this=serializer+0x10,args=(f.stream,obj))&255==1 and not f.errors,'original scalar writer')
        written=f.data;print('FUNCTION_LOADED_WORDS',loaded,flush=True)
        cleanup(f,(serializer,obj))
    check(set(f.allocations)==set(f.freed),'all tracked native scalar allocations released')
    capture=[mode,input_bytes.hex(),states,written.hex()]
    print('FUNCTION_CAPTURE',json.dumps(capture,allow_nan=False),flush=True)
    print(f'PASS {checks}/{checks}: original FunctionEval {mode}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
