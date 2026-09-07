#!/usr/bin/env python3
"""Finite PC plane/sphere and real-root selection vs portable partial class.

Includes separate literal NaN classification cases; constructor gap and CRT
seams inherited from VisibilityFixture. No portal/occluder/GPU claim.
"""
from pathlib import Path
import random
import struct
import subprocess
import sys
from pc_instruction_emulator import PcInstructions, ROOT, run_bounded
from probe_pc_visibility_runtime import CullingFixture

EXE = ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugVisibilityTests.exe'


def bits(value): return struct.unpack('<I',struct.pack('<f',value))[0]


def host(mode,rows):
    result=subprocess.run([str(EXE),mode],input='\n'.join(rows)+'\n',
                          text=True,capture_output=True,timeout=20,check=True)
    return [list(map(int,line.split(','))) for line in result.stdout.splitlines()]


def math_cases():
    p=PcInstructions()
    record,buffer,sphere=p.allocate(20),p.allocate(16*20),p.allocate(16)
    rng=random.Random(0x490500)
    rows,expected=[],[]
    cases=[]
    for index in range(256):
        count=index%9
        planes=[(tuple(rng.randrange(-32,33)/4 for _ in range(4)),rng.randrange(2))
                for _ in range(count)]
        values=tuple(rng.randrange(-64,65)/4 for _ in range(4))
        cases.append((planes,values,index%3))
    for z in (-2.,-1.,0.,1.,2.,float('nan')):
        for active in (0,1):
            cases.append(([((0.,0.,1.,0.),1)],(0.,0.,z,1.),active))
    for planes,values,active in cases:
        p.put_floats(sphere,values)
        p.put_uint(record+4,buffer if planes else 0)
        p.put_uint(record+8,buffer+len(planes)*20 if planes else 0)
        p.put_uint(record+16,active)
        row=[len(planes),active,*map(bits,values)]
        for i,(equation,enabled) in enumerate(planes):
            p.put_floats(buffer+i*20,equation)
            p.put_uint(buffer+i*20+16,enabled)
            row.extend((*map(bits,equation),enabled))
        rows.append(' '.join(map(str,row)))
        p.run(0x4902d0,this=record,args=(sphere,))
        outside=p.reg('EAX')&255
        p.run(0x490500,this=record,args=(sphere,))
        classification=p.reg('EAX')
        p.run(0x4903e0,this=record,args=(sphere,))
        expected.append([outside,classification,p.reg('EAX')&255])
    actual=host('--math-batch',rows)
    assert len(actual)==len(expected)
    for i,(a,e) in enumerate(zip(actual,expected)): assert a==e,(i,a,e)
    return len(expected)*3,len(expected)


def selection_cases():
    f=CullingFixture()
    p=f.p
    objects=[f.add(kind,(0.,0.,10.,1.)) for kind in
             ('partition','static','static','dynamic','dynamic','dynamic')]
    f.call(0x426740,this=f.root,args=(objects[1][0],))
    f.call(0x426690,this=f.root,args=(objects[3][0],))
    f.build()
    planes=f.entry_planes[0]
    rng=random.Random(0x46c4d0)
    rows,expected=[],[]
    for index in range(64):
        seed=(0,1,42,0xfffffffe,0xffffffff)[index%5]
        culling,debug=index%2,(index//2)%2
        p.put_uint(f.visibility+0x14,seed)
        p.mu.mem_write(f.visibility+0x3c,bytes([culling]))
        p.mu.mem_write(p.uint(0x75526c)+0x21,bytes([debug]))
        row=[seed,culling,debug]
        for equation,enabled in planes: row.extend((*map(bits,equation),enabled))
        for i,(obj,support) in enumerate(objects):
            mark=rng.choice((0,seed,(seed+1)&0xffffffff))
            enabled,bypass=rng.randrange(2),rng.randrange(2)
            sphere=(rng.randrange(-32,33),rng.randrange(-32,33),rng.randrange(-8,49),rng.randrange(5))
            p.put_uint(support+0x64,mark)
            p.put_floats(support+0x24,sphere)
            if i>=3:
                p.put_uint(obj+0xb0,(p.uint(obj+0xb0)&~0x200)|(enabled*0x200))
                p.mu.mem_write(obj+0x130,bytes([bypass]))
            row.extend((mark,enabled,bypass,*map(bits,sphere)))
        rows.append(' '.join(map(str,row)))
        output=f.build()
        ids=[next(i for i,pair in enumerate(objects) if pair[1]==support) for support in output]
        expected.append([p.uint(f.scene_object+0x40),len(ids),*ids,
                         *[p.uint(support+0x64) for obj,support in objects]])
    actual=host('--select-batch',rows)
    assert len(actual)==len(expected)
    for i,(a,e) in enumerate(zip(actual,expected)): assert a==e,(i,a,e)
    p.mu.mem_write(p.uint(0x75526c)+0x21,b'\0')
    f.close()
    return sum(map(len,expected)),len(expected)


def main():
    a,ac=math_cases()
    b,bc=selection_cases()
    print(f'PASS {a+b}/{a+b}: sphere {a}/{ac} cases; visibility selection {b}/{bc} cases')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
