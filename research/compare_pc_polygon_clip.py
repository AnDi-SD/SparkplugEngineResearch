#!/usr/bin/env python3
"""Original491AA0 ring clip versus geometry-only analytical C++ helper.

Independent64-case children, no split/resume of one native call. Random vertex
order deliberately includes nonconvex inputs; no native convexity validation
is assumed. Metadata is checked separately by probe_pc_polygon_clip.py.
"""
from pathlib import Path
import math
import random
import struct
import subprocess
import sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_polygon_clip import PortalSceneFixture,set_points,points,bits

EXE=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugPolygonClipTests.exe'


def floating(value):return struct.unpack('<f',struct.pack('<I',value))[0]


def main(part):
    f=PortalSceneFixture();p=f.p;src=f.visibility+0x54;dest=f.visibility+0x80
    record=p.allocate(16);rng=random.Random(0x491aa0+part);rows=[];expected=[]
    for index in range(64):
        count=index%11;keep=index%2;alias=(index//2)%2
        shape=[tuple(rng.randrange(-32,33)/8 for _ in range(3)) for _ in range(count)]
        plane=tuple(rng.randrange(-8,9)/4 for _ in range(4))
        epsilon=(.001,0.,-.001,.25)[index%4]
        if index<12:
            epsilon=.001;v=floating(bits(-epsilon))
            shape=[(v,0.,0.),(-2.,1.,0.),(2.,2.,0.),(v,3.,0.)]
            plane=((1.,0.,0.,0.),(0.,1.,0.,0.),(0.,0.,1.,0.),
                   (float('nan'),0.,0.,0.),(2.,0.,0.,0.),(-1.,0.,0.,0.))[index%6]
        set_points(f,src,shape);p.put_floats(record,plane)
        output=src if alias else dest
        result=f.call(0x491aa0,this=src,args=(record,keep,output,bits(epsilon)))&255
        native=points(p,output)
        expected.append((result,native))
        rows.append(' '.join(map(str,[len(shape),keep,alias,bits(epsilon),*map(bits,plane),
                                     *(bits(v) for point in shape for v in point)])))
    f.close()
    output=subprocess.run([str(EXE),'--batch'],input='\n'.join(rows)+'\n',text=True,
                          capture_output=True,timeout=20,check=True)
    lines=output.stdout.splitlines();assert len(lines)==len(expected)
    checks=0;exact=0;numeric=0
    for index,(line,(result,vertices)) in enumerate(zip(lines,expected)):
        values=list(map(int,line.split(',')))
        assert values[:2]==[result,len(vertices)],(part,index,values[:2],result,vertices,rows[index])
        checks+=2
        actual=[floating(word) for word in values[2:]];native=[v for point in vertices for v in point]
        assert len(actual)==len(native)
        for a,e in zip(actual,native):
            assert ((math.isnan(a) and math.isnan(e)) or math.isclose(a,e,rel_tol=3e-6,abs_tol=3e-6)), (part,index,a,e,rows[index])
            checks+=1;numeric+=1;exact+=bits(a)==bits(e)
    print(f'PASS {checks}/{checks}: polygon clip part{part},64 cases; numeric {exact}/{numeric} bit-exact, '
          'tolerance3e-6');return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(int(sys.argv[2])))
    for part in sys.argv[1:] or ('0','1','2','3'):
        result=run_bounded(Path(__file__),(str(part),))
        if result:raise SystemExit(result)
