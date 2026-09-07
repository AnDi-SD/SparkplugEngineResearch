#!/usr/bin/env python3
"""Original Octree construction, leaf query and spatial registrations.

Children/geometry/Zone presence are explicit decoded-record inputs. No whole
Scene/Visibility claim: fresh whole Octree traversal separately reached100k
inside45E870 plane-vector copy; isolated copy also capped and was not resumed.
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


def vector(p,obj,offset):
    begin,end=p.uint(obj+offset+4),p.uint(obj+offset+8)
    assert 0<=end-begin<=128 and (end-begin)%4==0
    return [p.uint(a) for a in range(begin,end,4)]


class OctreeFixture(SceneRenderFixture):
    def __init__(self):
        super().__init__()
        self.p.put_uint(0x75da88,0x21a70829);self.p.put_uint(0x75dad0,0x75e1b8)

    def octree(self):
        p=self.p;obj=self.object(0x41a760)
        p.put_floats(obj+0x84,(0.,0.,0.));p.put_floats(obj+0xb0,(-10.,-10.,-10.,10.,10.,10.))
        children=[]
        for i in range(8):
            child=self.object(0x426910);children.append(child);self.objects.remove(child)
            p.put_uint(p.uint(obj+0x58)+i*4,child);p.put_uint(child+0x54,obj)
        return obj,children

    def zone_presence(self,obj):
        # Explicit intrusive-reference fixture, not a recovered association setter.
        p=self.p;zone=p.uint(self.root+0x60)
        refs=p.uint(zone+8)&65535
        assert refs<65534 and p.uint(obj+0x60)==0
        p.mu.mem_write(zone+8,(refs+1).to_bytes(2,'little'));p.put_uint(obj+0x60,zone)
        return zone


def lifetime_and_leaf():
    f=OctreeFixture();p=f.p
    cold=f.object(0x41a760)
    check(f.allocations[cold]==0xc8 and p.uint(cold)==0x6e4420,'exactC8 primary33')
    slots=p.uint(cold+0x58)
    check(p.uint(cold+0x5c)==8 and f.allocations[slots]==32 and
          all(p.uint(slots+4*i)==0 for i in range(8)),'actual ctor owns eight initially null slots')
    check(f.words(cold,0x84,0xb0)==[0xcccccccc]*11 and f.words(cold,0xb0,0xc8)==[0]*6,
          'pivot and ray scratch uninitialized, mins/maxs zero')
    check(f.call(0x408370,this=cold,args=(0x67672341,))&255==1 and
          f.call(0x408370,this=cold,args=(0x695c0f65,))&255==0,'original direct Partition not Node')
    obj,children=f.octree();point=p.allocate(12)
    for i in range(8):
        p.put_floats(point,tuple(1. if i&(1<<axis) else -1. for axis in range(3)))
        check(f.call(0x449e50,this=obj,args=(point,0))==children[i],'actual recursive query exact octant')
    p.put_floats(point,(0.,0.,0.))
    check(f.call(0x449e50,this=obj,args=(point,0))==children[0],'pivot equality belongs to low child')
    zone=f.zone_presence(obj)
    check(f.call(0x449e50,this=obj,args=(point,1))==obj,'stopAtZone returns owning node before recursion')
    check(f.call(0x449e50,this=obj,args=(point,0))==children[0],'false stop flag ignores Zone')
    clone=f.call(0x41b100,this=obj);f.objects.append(clone)
    check(p.uint(clone+0x60)==0 and p.uint(clone+0x5c)==8 and
          f.words(clone,0x84,0xb0)==[0xcccccccc]*11 and
          all(p.uint(p.uint(clone+0x58)+4*i)==0 for i in range(8)),
          'original Base-only clone omits geometry, Zone and child graph')
    p.mu.mem_write(children[0]+8,(3).to_bytes(2,'little'))
    f.call(0x449ae0,this=obj,args=(1,));f.objects.remove(obj)
    check(all(child in f.freed for child in children),'direct child deletion ignores intrusive count')
    check(zone not in f.freed,'Octree releases its one Zone reference, Scene still owns Zone')
    f.close();check(slots in f.freed,'cold ctor slot-array freed normally')


def render_registration():
    f=OctreeFixture();p=f.p
    root,children=f.octree();zone=f.zone_presence(root)
    node=f.node()
    scenarios=[((4.,4.,4.,1.),0,True,[],[7]),
               ((0.,4.,4.,1.),0,True,[root],[]),
               ((0.,4.,4.,1.),0x400,True,[],[6,7]),
               ((0.,4.,4.,1.),0x100400,True,[root],[]),
               ((0.,4.,4.,1.),0,False,[],[6,7]),
               ((.7,.7,.9,1.),0x400,True,[],[4,5,6,7])]
    for sphere,flags,zoned,on_root,indices in scenarios:
        f.call(0x424dd0,this=node,args=(1,))
        p.put_floats(node+0xd8,sphere);p.put_uint(node+0xb0,0x200|flags)
        p.put_uint(root+0x60,zone if zoned else 0)
        f.call(0x449c10,this=root,args=(node,))
        expected=on_root+[children[i] for i in indices]
        check(vector(p,node,0x1c4)==expected,'actual dynamic/static/billboard/Zone registration branches')
        check(vector(p,root,0x20)==([node] if on_root else []) and
              all(vector(p,child,0x20)==([node] if i in indices else []) for i,child in enumerate(children)),
              'reciprocal links match the selected root/leaf subset')
    p.put_uint(root+0x60,zone)
    f.close()


def ray_candidates():
    f=OctreeFixture();p=f.p
    obj,children=f.octree();ray=p.allocate(24);output=p.allocate(4)
    for values,expected in (((-2.,-3.,-4.,1.,1.,1.),[(0,0.),(1,2.),(3,3.),(7,4.)]),
                            ((-2.,-2.,-2.,1.,1.,1.),[(0,0.),(7,2.)]),
                            ((2.,3.,4.,1.,1.,1.),[(7,0.)]),
                            ((-2.,-3.,-4.,0.,0.,0.),[(0,0.)])):
        p.put_floats(ray,values)
        count=f.call(0x449690,this=obj,args=(ray,output))
        check(p.uint(output)==obj+0x90 and count==len(expected),'ray query returns borrowed internal scratch/count')
        actual=[(p.uint(obj+0x90+i*8),p.floats(obj+0x94+i*8,1)[0]) for i in range(count)]
        check(actual==expected,'forward plane crossings sorted/deduplicated by octant')
    f.close()


def debug_scene_traversal():
    f=OctreeFixture();p=f.p
    root,children=f.octree()
    p.put_floats(root+0x84,(0.,0.,20.));p.put_floats(root+0xb0,(-100.,-100.,0.,100.,100.,100.))
    system=p.uint(f.scene_object+0x38);old=f.root;zone=p.uint(old+0x60)
    # Explicit decoded-graph replacement transfers the existing owned Zone ref.
    # No serializer/SetPartition reconstruction is claimed by this setup.
    p.put_uint(old+0x60,0);p.put_uint(root+0x60,zone)
    p.put_uint(p.uint(zone+0xb8),root);p.put_uint(system+0x1d4,root)
    p.put_uint(root+0x74,system);f.objects.remove(root);f.root=root
    f.call(0x4259e0,this=root,args=(f.scene_object,));f.call(0x426890,this=old,args=(1,))
    objects=[f.add('dynamic',((-4.,4.)[i&1],(-4.,4.)[(i>>1)&1],(10.,30.)[(i>>2)&1],1.))
             for i in range(8)]
    check(all(vector(p,child,0x20)==[objects[i][0]] for i,child in enumerate(children)),
          'actual Node world registration places one dynamic node in each leaf')
    debug=p.uint(0x75526c);p.mu.mem_write(debug+0x21,b'\1')
    check(f.draw()==1,'whole Scene executes real DEBUG unclipped Octree branch, not normal copy path')
    expected=[objects[i][1] for i in (0,1,2,4,3,5,6,7)]
    check([e[1] for e in f.events if e[0]=='node-draw']==expected,
          'actual unclipped recursion and support draw follow original order table')
    stamp=p.uint(f.scene_object+0x40)
    check(all(p.uint(child+0x7c)==stamp for child in children),'all eight leaves receive actual frame mark')
    p.mu.mem_write(debug+0x21,b'\0');f.close()


CASES={'lifetime':lifetime_and_leaf,'registration':render_registration,'ray':ray_candidates,
       'debug-scene':debug_scene_traversal}


def main(case):
    CASES[case]();print(f'PASS {checks}/{checks}: Octree {case}');return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    for case in sys.argv[1:] or CASES:
        result=run_bounded(Path(__file__),(case,))
        if result:raise SystemExit(result)
