#!/usr/bin/env python3
"""Original491660 count/ring,491A30 copy and491AA0 clip contracts.

Uses existing explicit CRT/device fixture, no app/OS/GPU execution. Original
value-class name remains unknown. No input>=128 or enlarged guest budget.
"""
from pathlib import Path
import struct
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_zone_portal_runtime import PortalSceneFixture

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]


def nodes(p,obj):
    count=p.uint(obj+8);head=p.uint(obj+0xc);node=head;result=[]
    assert count<=254
    for _ in range(count):
        result.append(node);node=p.uint(node+0x18)
    assert not count or node==head
    return result


def points(p,obj):return [p.floats(node+8,3) for node in nodes(p,obj)]


def set_points(f,obj,values):
    assert len(values)<=127
    f.call(0x491660,this=obj,args=(len(values),))
    for node,value in zip(nodes(f.p,obj),values):f.p.put_floats(node+8,value)


def clip(f,source,destination,plane,keep=False,epsilon=.001):
    record=f.p.allocate(16);f.p.put_floats(record,plane)
    return f.call(0x491aa0,this=source,args=(record,int(keep),destination,bits(epsilon)))&255


def ring_counts():
    f=PortalSceneFixture();p=f.p;obj=f.visibility+0x54
    check(p.uint(obj+8)==32 and len(nodes(p,obj))==32,'491660(32) changes logical count, not capacity only')
    set_points(f,obj,[(float(i),0.,0.) for i in range(8)])
    old=nodes(p,obj);ids=[p.uint(node+4) for node in old]
    f.call(0x491660,this=obj,args=(3,))
    check(nodes(p,obj)==old[:3] and points(p,obj)==[(float(i),0.,0.) for i in range(3)],
          'shrink removes tail, preserves first vertices')
    check(p.uint(old[0]+0x14)==old[2] and p.uint(old[2]+0x18)==old[0],
          'previous14/next18 close remaining ring')
    f.call(0x491660,this=obj,args=(8,));grown=nodes(p,obj)
    check(grown==old,'immediate growth reuses tail pool nodes in original order')
    check([p.uint(node+4) for node in grown[:3]]==ids[:3] and
          all(p.uint(node+4)>old_id for node,old_id in zip(grown[3:],ids[3:])),
          'reused nodes receive new global serials, surviving IDs unchanged')
    f.call(0x491660,this=obj,args=(0,))
    check(p.uint(obj+8)==0 and p.uint(obj+0xc)==old[0],'first shrink-to-zero leaves stale head')
    f.call(0x491660,this=obj,args=(0,))
    check(p.uint(obj+0xc)==0,'next resize sees old zero count and clears stale head')
    f.call(0x491660,this=obj,args=(4,))
    check(len(nodes(p,obj))==4,'regrowth after zero restores closed ring')
    f.close();check(True,'native scratch teardown and pool shutdown clean')


def copy_and_alias():
    f=PortalSceneFixture();p=f.p;src=f.visibility+0x54;dest=f.visibility+0x80
    shape=[(-2.,-2.,0.),(2.,-2.,0.),(2.,2.,0.),(-2.,2.,0.)]
    set_points(f,src,shape)
    for obj,seed in ((src,0x11110000),(dest,0x22220000)):
        for offset in (0,0x20,0x24,0x28):p.put_uint(obj+offset,seed+offset)
        p.put_floats(obj+0x10,(1.,2.,3.,4.) if obj==src else (5.,6.,7.,8.))
    identity=p.uint(dest+4);f.call(0x491a30,this=dest,args=(src,))
    check(points(p,dest)==shape and nodes(p,dest)!=nodes(p,src),'deep point copy, disjoint rings')
    check(p.floats(dest+0x10,4)==(1.,2.,3.,4.) and p.uint(dest+0x20)==0x11110020,
          'copy transfers plane10 and opaque20')
    check(p.uint(dest+4)==identity and [p.uint(dest+off) for off in (0,0x24,0x28)]==
          [0x22220000,0x22220024,0x22220028],'copy preserves ID and unrelated destination words')
    before=bytes(p.mu.mem_read(src,0x2c));f.call(0x491a30,this=src,args=(src,))
    check(bytes(p.mu.mem_read(src,0x2c))==before,'original self-copy no-op')
    check(clip(f,src,dest,(1.,0.,0.,0.))==1,'mixed out-of-place clipping')
    check(p.floats(dest+0x10,4)==(1.,2.,3.,4.) and p.uint(dest+0x24)==0x22220024,
          'mixed out-of-place clip leaves existing destination metadata')
    identity=p.uint(src+4)
    check(clip(f,src,src,(1.,0.,0.,0.))==1 and points(p,src)==points(p,dest),'in-place geometry same')
    check(p.uint(src+4)==identity and p.uint(src+0x20)==0x11110020,
          'in-place swap preserves destination ID and source opaque20')
    check(p.floats(src+0x10,4)==(0.,0.,0.,0.) and
          [p.uint(src+off) for off in (0,0x24,0x28)]==[0,0,0],
          'in-place mixed swap copies global scratch zero metadata, not source plane')
    check(p.uint(0x7629b8)==p.uint(0x7629bc)==0,'global scratch relinquishes count/head after in-place move')
    f.close();check(True,'native lifetime remains clean with explicit unknown scalar inputs')


def epsilon_contract():
    f=PortalSceneFixture();p=f.p;src=f.visibility+0x54;dest=f.visibility+0x80
    eps=p.floats(0x6dc3a8,1)[0]
    shape=[(-eps,0.,0.),(-2.,1.,0.),(2.,2.,0.),(-eps,3.,0.)]
    set_points(f,src,shape)
    check(clip(f,src,dest,(1.,0.,0.,0.))==1,'mixed with two epsilon-on vertices accepted')
    out=points(p,dest)
    check(out==[(0.,0.,0.),(0.,1.5,0.),(2.,2.,0.),(-eps,3.,0.)],
          'postpass repeatedly corrects first point, does not advance to final on-point')
    check(points(p,src)==shape,'out-of-place source unchanged')
    for values,label in (([(0.,0.,0.),(0.,1.,0.),(0.,1.,1.)],'coplanar'),([], 'empty')):
        set_points(f,src,values)
        check(clip(f,src,dest,(1.,0.,0.,0.),False)==0 and points(p,dest)==[],label+' flag0 clears/false')
        check(clip(f,src,dest,(1.,0.,0.,0.),True)==1 and points(p,dest)==values,label+' flag1 copies/true')
    set_points(f,src,[(float(i),1.,2.) for i in range(1,128)])
    check(clip(f,src,dest,(1.,0.,0.,0.))==1 and len(points(p,dest))==127,
          '127 points fit native128-entry arrays including closing duplicate; 128 intentionally not executed')
    f.close()


CASES={'counts':ring_counts,'copy':copy_and_alias,'epsilon':epsilon_contract}


def main(case):
    CASES[case]();print(f'PASS {checks}/{checks}: PC polygon {case}');return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    for case in sys.argv[1:] or CASES:
        result=run_bounded(Path(__file__),(case,))
        if result:raise SystemExit(result)
