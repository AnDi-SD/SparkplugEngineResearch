#!/usr/bin/env python3
"""Original PC BSP lifetime/geometry/queries/registrations and Scene Zone choice.

Explicit decoded owned graph, unchanged100k/2sec per call and30sec child.
No protected45E870 resumption/replacement; normal whole Scene starts at the
camera's Zone leaf, not at an invented BSP root holding all room objects.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_render_runtime import SceneRenderFixture

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def pointers(p,obj,offset):
    begin,end=p.uint(obj+offset+4),p.uint(obj+offset+8)
    assert 0<=end-begin<=128 and (end-begin)%4==0
    return [p.uint(a) for a in range(begin,end,4)]


class BspFixture(SceneRenderFixture):
    def __init__(self):
        super().__init__();self.p.put_uint(0x7613f8,0x7362ab22);self.p.put_uint(0x761440,0x75e1b8)

    def bsp(self):
        p=self.p;obj=self.object(0x480b90);children=[]
        plane=p.allocate(16);p.put_floats(plane,(1.,0.,0.,0.))
        self.call(0x44cda0,this=obj,args=(plane,))
        for i in range(2):
            child=self.object(0x426910);children.append(child);self.objects.remove(child)
            p.put_uint(p.uint(obj+0x58)+4*i,child);p.put_uint(child+0x54,obj)
        return obj,children


def lifetime_and_geometry():
    f=BspFixture();p=f.p;obj=f.object(0x480b90)
    check(f.allocations[obj]==0xac and p.uint(obj)==0x6eba30,'exactAC primary33')
    array=p.uint(obj+0x58)
    check(p.uint(obj+0x5c)==2 and f.allocations[array]==8 and p.uint(array)==p.uint(array+4)==0,
          'two directly owned initially-null child slots')
    check(f.words(obj,0x84,0xa4)==[0xcccccccc]*8 and f.words(obj,0xa4,0xac)==[0,0],
          'plane and two ray records untouched, optional polygon empty')
    check(f.call(0x408370,this=obj,args=(0x67672341,))&255==1 and
          f.call(0x408370,this=obj,args=(0x695c0f65,))&255==0,'original Partition, not Node')
    plane=p.allocate(16);p.put_floats(plane,(2.,3.,4.,5.));f.call(0x44cda0,this=obj,args=(plane,))
    check(p.floats(obj+0x84,4)==(2.,3.,4.,5.),'actual reader setter copies raw plane, no normalization')
    source=p.allocate(60);values=(0.,0.,0.,1.,0.,0.,0.,1.,0.,1.,1.,9.,2.,3.,4.)
    p.put_floats(source,values);old=0
    for count in (3,5,0):
        f.call(0x480210,this=obj,args=(count,source));owned=p.uint(obj+0xa4)
        check(p.uint(obj+0xa8)==count and f.allocations[owned]==count*12 and
              (not count or p.floats(owned,count*3)==values[:count*3]),'deep optional polygon copy/count')
        check(not old or old in f.freed,'previous polygon released on reinit')
        check(p.floats(obj+0x84,4)==(2.,3.,4.,5.),'polygon setter does NOT derive/alter split plane')
        old=owned
    clone=f.call(0x480c10,this=obj);f.objects.append(clone)
    check(f.words(clone,0x84,0xa4)==[0xcccccccc]*8 and f.words(clone,0xa4,0xac)==[0,0],
          'Base-only clone drops plane/polygon/ray state')
    f.close();check(array in f.freed and old in f.freed,'owned arrays freed on original destruction')


def leaf_and_ray():
    f=BspFixture();p=f.p;obj,children=f.bsp();point=p.allocate(12)
    for x,child in ((1.,0),(-1.,1),(0.,1),(float('nan'),1)):
        p.put_floats(point,(x,0.,0.))
        check(f.call(0x480710,this=obj,args=(point,0))==children[child],
              'strict positive slot0, zero/negative/unordered slot1')
    zone=p.uint(f.root+0x60);refs=p.uint(zone+8)&65535
    p.mu.mem_write(zone+8,(refs+1).to_bytes(2,'little'));p.put_uint(obj+0x60,zone)
    check(f.call(0x480710,this=obj,args=(point,1))==obj,'stopAtZone returns before plane read')
    ray=p.allocate(24);output=p.allocate(4)
    for values,expected in (((-2.,0.,0.,1.,0.,0.),[(1,0.),(0,2.)]),
                            ((2.,0.,0.,-1.,0.,0.),[(0,0.),(1,2.)]),
                            ((2.,0.,0.,1.,0.,0.),[(0,0.)]),
                            ((0.,0.,0.,1.,0.,0.),[(1,0.),(0,-0.)]),
                            ((-2.,0.,0.,0.,1.,0.),[(1,0.)])):
        p.put_floats(ray,values);count=f.call(0x480360,this=obj,args=(ray,output))
        check(p.uint(output)==obj+0x94 and count==len(expected),'borrowed internal94 two ray candidates')
        actual=[(p.uint(obj+0x94+8*i),p.floats(obj+0x98+8*i,1)[0]) for i in range(count)]
        check(actual==expected,'native forward crossing, side, parallel and zero-origin cases')
    f.close()


def registrations():
    f=BspFixture();p=f.p;obj,children=f.bsp();node=f.node()
    zone=p.uint(f.root+0x60);refs=p.uint(zone+8)&65535
    p.mu.mem_write(zone+8,(refs+1).to_bytes(2,'little'));p.put_uint(obj+0x60,zone)
    cases=[((4.,0.,0.,1.),0,True,[children[0]]),((-4.,0.,0.,1.),0,True,[children[1]]),
           ((1.,0.,0.,1.),0,True,[obj]),((1.,0.,0.,1.),0x400,True,children),
           ((1.,0.,0.,1.),0x100400,True,[obj]),((1.,0.,0.,1.),0,False,children)]
    for sphere,flags,zoned,expected in cases:
        f.call(0x424dd0,this=node,args=(1,));p.put_floats(node+0xd8,sphere)
        p.put_uint(node+0xb0,0x200|flags);p.put_uint(obj+0x60,zone if zoned else 0)
        f.call(0x480540,this=obj,args=(node,))
        check(pointers(p,node,0x1c4)==expected,'actual BSP dynamic/static/billboard/Zone reciprocal placement')
        check(all(pointers(p,root,0x20)==([node] if root in expected else []) for root in [obj,*children]),
              'all root/child memberships agree')
    p.put_uint(obj+0x60,zone);f.close()


def scene_zone_selection():
    f=BspFixture();p=f.p;root=f.object(0x480b90);system=p.uint(f.scene_object+0x38)
    old=f.root;other=f.object(0x426910);zone=f.object(0x480fd0)
    plane=p.allocate(16);p.put_floats(plane,(1.,0.,0.,0.));f.call(0x44cda0,this=root,args=(plane,))
    p.put_uint(p.uint(root+0x58),old);p.put_uint(p.uint(root+0x58)+4,other)
    for child in (old,other):p.put_uint(child+0x54,root)
    p.put_uint(system+0x1d4,root);p.put_uint(root+0x74,system)
    f.objects.remove(root);f.objects.remove(other)
    # Zone tree attachment and borrowed local-root append execute original code.
    f.call(0x421a60,this=system,args=(zone,));f.objects.remove(zone)
    f.call(0x481080,this=zone,args=(other,))
    refs=p.uint(zone+8)&65535;p.mu.mem_write(zone+8,(refs+1).to_bytes(2,'little'));p.put_uint(other+0x60,zone)
    f.call(0x4259e0,this=root,args=(f.scene_object,));f.root=root
    positive,ps=f.add('dynamic',(4.,0.,20.,.25));negative,ns=f.add('dynamic',(-4.,0.,20.,.25))
    check(pointers(p,old,0x20)==[positive] and pointers(p,other,0x20)==[negative],
          'actual Node world registrations separate the two BSP leaves')
    for x,expected in ((1.,ps),(-1.,ns),(0.,ns)):
        p.put_floats(f.native_camera+0x20,(x,0.,0.));f.call(0x428af0,this=f.native_camera,args=(1,))
        check(f.draw()==1 and [e[1] for e in f.events if e[0]=='node-draw']==[expected],
              'whole Scene chooses camera leaf Zone and draws only its local root without portals')
    f.close();check(True,'whole BSP/Zone/Node graph destroyed normally')


CASES={'lifetime':lifetime_and_geometry,'queries':leaf_and_ray,'registration':registrations,'scene':scene_zone_selection}


def main(case):
    CASES[case]();print(f'PASS {checks}/{checks}: PC BSP {case}');return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    for case in sys.argv[1:] or CASES:
        result=run_bounded(Path(__file__),(case,))
        if result:raise SystemExit(result)
