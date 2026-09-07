#!/usr/bin/env python3
"""Original BSP query instructions versus partial C++ source, bounded records.

No constructor/whole Scene claim here. Quantized random inputs and explicit
epsilon/NaN cases; ray float tolerance is not universal x87 equivalence.
"""
from pathlib import Path
import math
import random
import struct
import subprocess
import sys
from pc_instruction_emulator import PcInstructions,ROOT,run_bounded

EXE=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugBspTests.exe'


def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
def floating(word):return struct.unpack('<f',struct.pack('<I',word))[0]


def main(part):
    p=PcInstructions()
    node,children,point,sphere,camera,ray,out,record,buffer,polygon=[
        p.allocate(size) for size in (0xac,8,12,16,0x238,24,4,20,320,192)]
    leaves=[p.allocate(0x84) for _ in range(2)]
    p.put_uint(node,0x6eba30);p.put_uint(node+0x58,children);p.put_uint(node+0x60,0)
    for i,leaf in enumerate(leaves):
        p.put_uint(children+4*i,leaf);p.put_uint(leaf,0x6dcb08)
    p.put_uint(node+0xa4,polygon)
    rng=random.Random(0x480c60+part);rows=[];expected=[]
    for index in range(128):
        plane=tuple(rng.randrange(-8,9)/4 for _ in range(4))
        vertices=[tuple(rng.randrange(-32,33)/4 for _ in range(3)) for _ in range(index%13)]
        position=tuple(rng.randrange(-32,33)/4 for _ in range(3))
        ball=tuple(rng.randrange(-24,25)/4 for _ in range(4))
        view=tuple(rng.randrange(-32,33)/4 for _ in range(3))
        ray_values=tuple(rng.randrange(-24,25)/4 for _ in range(6))
        planes=[(tuple(rng.randrange(-8,9)/4 for _ in range(4)),rng.randrange(2))
                for _ in range(index%7)]
        active=index%4
        if index<16:
            plane=(1.,0.,0.,0.)
            value=(-.001,0.,.001,float('nan'))[index%4]
            position=(value,0.,0.);view=position
            ball=((1.,0.,0.,1.),(-1.,0.,0.,1.),(0.,0.,0.,-1.),
                  (float('nan'),0.,0.,1.))[index%4]
            ray_values=((0.,0.,0.,1.,0.,0.),(-2.,0.,0.,1e-5,0.,0.),
                        (2.,0.,0.,-1e-5,0.,0.),(-2.,0.,0.,1.,0.,0.))[index%4]
            planes=[((0.,0.,1.,value),index%2)]
        p.put_floats(node+0x84,plane);p.put_uint(node+0xa8,len(vertices))
        for i,vertex in enumerate(vertices):p.put_floats(polygon+12*i,vertex)
        p.put_floats(point,position);p.put_floats(sphere,ball);p.put_floats(camera+0x74,view)
        p.put_floats(ray,ray_values)
        p.put_uint(record+4,buffer);p.put_uint(record+8,buffer+20*len(planes));p.put_uint(record+16,active)
        row=[len(vertices),*map(bits,plane)]
        for vertex in vertices:row.extend(map(bits,vertex))
        row.extend((*map(bits,(*position,*ball,*view,*ray_values)),len(planes),active))
        for i,(equation,enabled) in enumerate(planes):
            p.put_floats(buffer+20*i,equation);p.put_uint(buffer+20*i+16,enabled)
            row.extend((*map(bits,equation),enabled))
        result=[]
        p.run(0x480710,this=node,args=(point,0));result.append(leaves.index(p.reg('EAX')))
        p.run(0x480780,this=node,args=(point,));result.append(p.reg('EAX')&255)
        p.run(0x4807d0,this=node,args=(sphere,));result.append(p.reg('EAX')&255)
        p.run(0x480c60,this=node,args=(record,camera));result.append(p.reg('EAX'))
        p.run(0x480360,this=node,args=(ray,out));count=p.reg('EAX');result.append(count)
        assert p.uint(out)==node+0x94 and count in (1,2)
        for i in range(count):result.extend((p.uint(node+0x94+8*i),p.uint(node+0x98+8*i)))
        rows.append(' '.join(map(str,row)));expected.append(result)
    output=subprocess.run([str(EXE),'--batch'],input='\n'.join(rows)+'\n',text=True,
                          capture_output=True,timeout=20,check=True)
    actual=[list(map(int,line.split(','))) for line in output.stdout.splitlines()]
    assert len(actual)==len(expected)
    exact=0;numeric=0
    for index,(a,e) in enumerate(zip(actual,expected)):
        assert len(a)==len(e) and a[:5]==e[:5],(part,index,a,e,rows[index])
        for j in range(5,len(e),2):
            assert a[j]==e[j],(part,index,a,e)
            x,y=floating(a[j+1]),floating(e[j+1]);numeric+=1;exact+=a[j+1]==e[j+1]
            assert (math.isnan(x) and math.isnan(y)) or math.isclose(x,y,rel_tol=2e-6,abs_tol=2e-6),(
                part,index,a,e,rows[index])
    count=sum(map(len,expected))
    print(f'PASS {count}/{count}: BSP source part{part},128 cases, ray {exact}/{numeric} bit-exact; tolerance2e-6')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(int(sys.argv[2])))
    for part in sys.argv[1:] or ('0','1'):
        result=run_bounded(Path(__file__),(str(part),))
        if result:raise SystemExit(result)
