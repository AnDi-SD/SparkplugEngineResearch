#!/usr/bin/env python3
"""Bounded original471420 first-three plane vs partial C++ portal source.

Finite quantized points plus tiny/collinear literals; per-component tolerance,
not universal x87 bit equivalence. Native borrowed plane/point records only.
"""
from pathlib import Path
import math
import random
import struct
import subprocess
import sys
from pc_instruction_emulator import PcInstructions,ROOT,run_bounded

EXE=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugZonePortalTests.exe'


def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
def floating(value):return struct.unpack('<f',struct.pack('<I',value))[0]


def main(part):
    p=PcInstructions();points=p.allocate(36);plane=p.allocate(16)
    rng=random.Random(0x481130+part);rows=[];expected=[]
    for index in range(128):
        values=[rng.randrange(-100,101)/8 for _ in range(9)]
        if index<8:
            side=(0.,.01,.031622,.031623,.1,1.,-1.,2.)[index]
            values=(0.,0.,3.,side,0.,3.,0.,side,3.)
        p.put_floats(points,values)
        p.run(0x471420,this=plane,args=(points,points+12,points+24))
        expected.append(p.floats(plane,4));rows.append(' '.join(map(str,map(bits,values))))
    output=subprocess.run([str(EXE),'--batch'],input='\n'.join(rows)+'\n',text=True,
                          capture_output=True,timeout=20,check=True)
    actual=[tuple(floating(int(word)) for word in line.split(',')) for line in output.stdout.splitlines()]
    assert len(actual)==len(expected)
    exact=0
    for index,(a,e) in enumerate(zip(actual,expected)):
        for component,(x,y) in enumerate(zip(a,e)):
            assert math.isclose(x,y,rel_tol=2e-6,abs_tol=2e-6),(part,index,component,a,e)
            exact+=bits(x)==bits(y)
    print(f'PASS {len(expected)*4}/{len(expected)*4}: portal plane part{part}, {len(expected)} cases, '
          f'{exact} bit-exact; tolerance2e-6');return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(int(sys.argv[2])))
    for part in sys.argv[1:] or ('0','1'):
        result=run_bounded(Path(__file__),(str(part),))
        if result:raise SystemExit(result)
