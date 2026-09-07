#!/usr/bin/env python3
"""Native Octree query math vs partial original-named C++ classes.

Borrowed C8/plane/camera records, original executable instructions only; no
whole Scene, protected plane-vector copy or object-constructor claim here.
Quantized finite corpus plus literal boundary/NaN inputs, not universal x87
bit-perfect equivalence. Each subset is an independent bounded process.
"""
from pathlib import Path
import random
import struct
import subprocess
import sys
from pc_instruction_emulator import PcInstructions,ROOT,run_bounded

EXE=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugOctreeTests.exe'


def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]


def main(part):
    p=PcInstructions()
    node,point,sphere,camera,record,buffer=[p.allocate(size) for size in (0xc8,12,16,0x238,20,320)]
    p.put_uint(node,0x6e4420)
    rng=random.Random(0x44aa30+part)
    rows,expected=[],[]
    for index in range(160):
        pivot=tuple(rng.randrange(-8,9)/4 for _ in range(3))
        mins=tuple(x-rng.randrange(1,25)/2 for x in pivot)
        maxs=tuple(x+rng.randrange(1,25)/2 for x in pivot)
        position=tuple(rng.randrange(-64,65)/4 for _ in range(3))
        values=tuple(rng.randrange(-48,49)/4 for _ in range(4))
        view=tuple(rng.randrange(-40,41)/4 for _ in range(3))
        count=index%9;active=index%4;child=index%8
        planes=[(tuple(rng.randrange(-12,13)/4 for _ in range(4)),rng.randrange(2)) for _ in range(count)]
        if index<16:
            pivot=(0.,0.,0.);mins=(-10.,)*3;maxs=(10.,)*3
            position=(0.,0.,(-.001,0.,.001,float('nan'))[index%4])
            values=((.7,.7,.9,1.),(.9,.7,.7,1.),(.7,.9,.7,1.),
                    (1.,1.,1.,1.),(0.,0.,0.,0.),(0.,0.,float('nan'),1.),
                    (0.,0.,2.,-1.),(1.,1.,1.,float('nan')))[index%8]
            planes=[((0.,0.,1.,(-.001,0.,.001,float('nan'))[index%4]),1)]
        p.put_floats(node+0x84,pivot);p.put_floats(node+0xb0,(*mins,*maxs))
        p.put_floats(point,position);p.put_floats(sphere,values);p.put_floats(camera+0x74,view)
        p.put_uint(record+4,buffer);p.put_uint(record+8,buffer+20*len(planes));p.put_uint(record+16,active)
        row=[*map(bits,(*pivot,*mins,*maxs,*position,*values,*view)),len(planes),active,child]
        for i,(equation,enabled) in enumerate(planes):
            p.put_floats(buffer+i*20,equation);p.put_uint(buffer+i*20+16,enabled)
            row.extend((*map(bits,equation),enabled))
        result=[]
        p.run(0x4494e0,args=(point,node+0x84),callee_pop=False);octant=p.reg('EAX');result.append(octant)
        p.run(0x449430,this=node,args=(point,));result.append(p.reg('EAX')&255)
        p.run(0x449530,this=node,args=(sphere,));result.append(p.reg('EAX')&255)
        p.run(0x449520,this=node,args=(octant,));result.append(p.reg('EAX'))
        p.run(0x44aa30,this=node,args=(record,camera));result.append(p.reg('EAX'))
        p.run(0x44ac60,this=node,args=(child,record))
        result.extend((p.uint(record+16),*[p.uint(buffer+i*20+16)&255 for i in range(len(planes))]))
        rows.append(' '.join(map(str,row)));expected.append(result)
    output=subprocess.run([str(EXE),'--batch'],input='\n'.join(rows)+'\n',text=True,
                          capture_output=True,timeout=20,check=True)
    actual=[list(map(int,line.split(','))) for line in output.stdout.splitlines()]
    assert len(actual)==len(expected)
    for i,(a,e) in enumerate(zip(actual,expected)):
        assert a==e,(part,i,a,e,rows[i])
    count=sum(map(len,expected))
    print(f'PASS {count}/{count}: original Octree/source part{part}, {len(expected)} cases')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(int(sys.argv[2])))
    for part in sys.argv[1:] or ('0','1'):
        result=run_bounded(Path(__file__),(str(part),))
        if result:raise SystemExit(result)
