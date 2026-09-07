#!/usr/bin/env python3
"""Actual DXLight factory/copy and device-payload producer; no GPU/internal seams."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup,bits

def main(mode,return_capture=False):
    if mode not in ('factory','copy','clone','directional','point','spot','unknown','world'):raise ValueError('bounded DXLight families')
    f=PCWriteBytesFixture();p=f.p;obj=f.call(0x4ac000);owned=[obj];checks=0;maximum=sum(p.visits.values());captures=[]
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    def snapshot(target):return [p.uint(target+0xd8),[p.uint(target+0xf0+4*i) for i in range(26)]]
    if f.allocations[obj]!=0x158 or p.uint(obj)!=0x6f0c88 or p.uint(obj+0xb4)!=0x6f0c84:raise AssertionError('actual DXLight158/vptrs')
    checks+=1
    for record,identity,parent in ((0x7634b0,0x6b3e7baa,0x75e278),(0x75e278,0x72444900,0x75dd88),(0x75dd88,0x695c0f65,0x7555f8),(0x7555f8,0x44de07fd,0x755310),(0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    p.put_uint(0x73fe98,0x7f234567);p.put_floats(0x7600e0,(.125,.25,.5))
    p.put_floats(obj+0x74,(4.,5.,6.));p.put_floats(obj+0xa4,(1.,2.,3.))
    if mode=='factory':captures.append(snapshot(obj))
    elif mode in ('copy','clone'):
        p.put_uint(obj+0xd8,bits(3.25))
        for i in range(26):p.put_uint(obj+0xf0+4*i,0x10000000+i)
        if mode=='copy':
            target=call(0x4ac000);owned.append(target);p.put_uint(target+0xd8,bits(9.5))
            if call(0x4b5910,this=obj,args=(target,))&255!=1:raise AssertionError('native DX copy')
        else:
            call(0x52fd90,this=0x755588);target=call(0x4ac240,this=obj);owned.append(target);call(0x6d7db0)
        captures.append(snapshot(target));checks+=1
    elif mode=='world':
        for inherited,stored,enabled in ((0,0,1),(8,0,1),(0,8,1),(0,8,0),(1,0,1)):
            p.put_uint(obj+0xb0,0x70a00|stored);p.mu.mem_write(obj+0xed,bytes([enabled]));p.mu.mem_write(obj+0xf0,b'\xcc'*104)
            call(0x4b58d0,this=obj,args=(inherited,));checks+=1
            captures.append([inherited,stored,enabled,bool(p.visits.get(0x4b53c0)),snapshot(obj)])
    else:
        kind={'directional':0,'point':1,'spot':2,'unknown':3}[mode];p.put_uint(obj+0xc0,kind)
        for attenuation,range_value,intensity in ((0,200.,1.),(1,200.,2.),(0,10.,-2.),(1,0.,2.),(1,200.,0.),(1,-5.,3.)):
            p.mu.mem_write(obj+0xf0,b'\xcc'*104);p.mu.mem_write(obj+0xd4,bytes([attenuation]));p.put_floats(obj+0xc4,(.25,.75,1.5,.5))
            p.put_uint(obj+0xd8,bits(intensity));p.put_floats(obj+0xe0,(range_value,.5,1.))
            call(0x4b53c0,this=obj);checks+=1;captures.append([attenuation,bits(range_value),bits(intensity),snapshot(obj)])
    cleanup(f,owned);checks+=1;capture=[mode,captures]
    if not return_capture:print('DX_LIGHT_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: actual DXLight {mode}; maxInstructions={maximum};heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
