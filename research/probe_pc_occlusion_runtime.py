#!/usr/bin/env python3
"""Original PC Occlusion lifetime/world/plane consumer, explicit Init gap.

Full470FE0 hit the unchanged 100k instruction cap in protected geometry work.
The prepared-buffer and cached-silhouette records below are explicit inputs,
not a successful Init or real asset load. CPU buffer readers, weld/comparator,
world updates, plane builder, whole Scene consumer and teardown are original.
"""
from pathlib import Path
import math
import struct
import sys
from pc_instruction_emulator import run_bounded
from pc_qsort_u16_fixture import install_u16_qsort_fixture
from probe_pc_scene_render_runtime import SceneRenderFixture

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


class OcclusionFixture(SceneRenderFixture):
    def __init__(self):
        super().__init__()
        self.p.put_uint(0x760640,0x43d24430)
        self.p.put_uint(0x760688,0x75dd88)
        self.sorts=install_u16_qsort_fixture(self.p,self.RX+0x40)

    def cpu_buffer(self,vertices=None,indices=None):
        p=self.p
        if vertices is not None:
            size,ctor,reader=0x5c,0x45fe50,0x460300
            data=struct.pack('<3I',0,len(vertices),0)+b''.join(struct.pack('<3f',*v) for v in vertices)
        else:
            size,ctor,reader=0x28,0x45f7f0,0x45fb80
            data=struct.pack('<3I',2,len(indices)//3,0)+struct.pack('<'+'H'*len(indices),*indices)
        p.run(0x4123d0,args=(size,),callee_pop=False)
        obj=p.reg('EAX')
        self.call(ctor,this=obj)
        self.objects.append(obj)
        self.data=data;self.position=0
        check(self.call(reader,this=obj,args=(self.stream,))&255==1 and self.position==len(data),
              'original CPU buffer reads the complete explicitly encoded stream')
        return obj

    def prepared_occluder(self):
        p=self.p
        obj=self.object(0x470a70)
        vertices=[(-2.,-2.,10.),(2.,-2.,10.),(2.,2.,10.),(-2.,2.,10.)]
        for offset,buffer in ((0xc8,self.cpu_buffer(vertices=vertices)),
                              (0xcc,self.cpu_buffer(vertices=vertices)),
                              (0xd0,self.cpu_buffer(indices=[0,1,2,0,2,3]))):
            check(p.uint(obj+offset)==0,'fixture fills an initially empty owned buffer slot')
            p.put_uint(obj+offset,buffer)
            self.objects.remove(buffer) # exact destructor owns these CPU buffers
        p.put_floats(obj+0x194,(0.,0.,10.,math.sqrt(8)))
        p.mu.mem_write(obj+0x1b4,b'\1') # explicit prepared-state input; NOT Init result
        return obj

    def attach(self,obj):
        self.call(0x421a60,this=self.p.uint(self.scene_object+0x14),args=(obj,))
        self.objects.remove(obj)
        self.call(0x45a7d0,this=self.p.uint(0x75db90))

    def cache_silhouette(self,obj,border=None,faces=None):
        """Explicit already computed camera-facing geometry for4702E0.

        This does not claim to reproduce46FFE0's edge-graph selection.
        """
        p=self.p
        border=border if border is not None else [(-2.,-2.,10.),(-2.,2.,10.),
                (2.,2.,10.),(2.,-2.,10.),(-2.,-2.,10.)]
        faces=faces if faces is not None else [(0.,0.,1.,10.)]
        for offset,values in ((0xe8,border),(0x108,faces)):
            check(p.uint(obj+offset+4)==0,'cached fixture vector initially empty')
            if values:
                flat=[n for row in values for n in row]
                buffer=self.buffer(flat)
                for delta,word in ((4,buffer),(8,buffer+len(flat)*4),(12,buffer+len(flat)*4)):
                    p.put_uint(obj+offset+delta,word)
        p.put_floats(obj+0x118,p.floats(self.native_camera+0x74,3))
        p.mu.mem_write(obj+0x1b5,b'\0')


def lifetime_and_world():
    f=OcclusionFixture();p=f.p
    obj=f.object(0x470a70)
    check(f.allocations[obj]==0x1b8 and p.uint(obj)==0x6e8de0,'actual exact1B8 and primary table')
    check(f.words(obj,0xb8,0xc8)==[0]*4 and f.words(obj,0xc8,0xd4)==[0]*3,
          'reverse links, stamp and three owned buffers start empty')
    check(p.uint(obj+0x1b4)==0xcccc0000,'initialized and camera-dirty false; padding untouched')
    check(f.call(0x4702e0,this=obj,args=(f.native_camera,))&255==0,'uninitialized plane build returns false')
    original=f.prepared_occluder()
    buffers=[p.uint(original+i) for i in (0xc8,0xcc,0xd0)]
    p.put_floats(original+0x20,(1.,2.,3.));p.put_floats(original+0x30,(-2.,3.,4.))
    f.attach(original)
    world_vb=p.uint(original+0xcc)
    check(p.floats(p.uint(world_vb+0x54),12)==(5.,-4.,43.,-3.,-4.,43.,-3.,8.,43.,5.,8.,43.),
          'original world update transforms every position through signed local scale and translation')
    sphere=p.floats(original+0x1a4,4)
    check(sphere[:3]==(1.,2.,43.) and abs(sphere[3]-math.sqrt(8)*4)<1e-5,
          'world sphere center transforms and radius uses maximum absolute scale')
    check(p.uint(f.root+0x44)!=0 and p.uint(p.uint(f.root+0x44))==original and
          p.uint(p.uint(original+0xb8))==f.root,'actual world registers reciprocal borrowed occlusion links')
    check(p.uint(original+0x1b4)&0xffff==0x101,'world marks camera-facing geometry dirty')
    f.call(0x421640,this=original,args=(0,0))
    check(not(p.uint(original+0xb0)&0x200) and p.uint(f.root+0x48)-p.uint(f.root+0x44)==4,
          'inherited SetEnabled does not unregister Occlusion unlike RenderNode override')
    clone=f.call(0x470ad0,this=original);f.objects.append(clone)
    check(p.uint(clone)==0x6e8de0 and f.words(clone,0xc8,0xd4)==[0]*3 and
          p.uint(clone+0x1b4)&255==0,'actual Node-only clone omits prepared geometry')
    f.call(0x46dcf0,this=original,args=(1,))
    check(p.uint(original+0xbc)==p.uint(original+0xb8) and p.uint(f.root+0x48)==p.uint(f.root+0x44),
          'notifying reverse drain removes root-side reference')
    f.close()
    check(all(buffer in f.freed for buffer in buffers),'normal original destructor releases all three owned buffers')


def weld():
    f=OcclusionFixture();p=f.p
    positions=[(-10.,-10.,10.),(10.,-10.,10.),(10.,10.,10.),(-10.,10.,10.),(-10.,-10.,10.)]
    ib=f.cpu_buffer(indices=[0,1,2,4,2,3]);vb=f.cpu_buffer(vertices=positions)
    helper=p.allocate(8);f.call(0x4604f0,this=helper)
    f.call(0x460d90,this=helper,args=(ib,vb))
    check(struct.unpack('<6H',p.mu.mem_read(p.uint(ib+0x24),12))==(0,1,2,0,2,3),
          'actual UInt16 weld remaps duplicate vertex references')
    check(p.uint(vb+0x1c)==4 and p.floats(p.uint(vb+0x54),12)==tuple(n for row in positions[:4] for n in row),
          'actual compaction retains four used positions in original order')
    check(len(f.sorts)==1 and f.sorts[0][1:]==(5,2,0x4607f0),'explicit qsort fixture calls original vertex comparator')
    check(p.uint(0x75ff9c)==vb,'shared comparator context is the input VB, not thread-local')
    # Independent comparator calls demonstrate byte-wise, not numeric/epsilon equality.
    ids=p.allocate(4);p.mu.mem_write(ids,struct.pack('<2H',0,1))
    p.run(0x4607f0,args=(ids,ids+2),callee_pop=False)
    check(p.reg('EAX')==1,'negative10 sorts after positive10 by little-endian bytes')
    p.put_floats(p.uint(vb+0x54),(0.,0.,0.,-0.,0.,0.))
    p.run(0x4607f0,args=(ids,ids+2),callee_pop=False)
    check(p.reg('EAX')==0xffffffff,'positive and negative zero positions are byte-distinct')
    f.call(0x460500,this=helper);f.close()


def cached_planes_and_scene():
    f=OcclusionFixture();p=f.p
    occluder=f.prepared_occluder();f.attach(occluder);f.cache_silhouette(occluder)
    check(f.call(0x4702e0,this=occluder,args=(f.native_camera,))&255==1,'actual plane builder accepts explicit cached silhouette')
    begin,end=p.uint(occluder+0x16c),p.uint(occluder+0x170)
    planes=[p.floats(at,4) for at in range(begin,end,20)]
    check(len(planes)==5 and p.uint(occluder+0x178)==5,'one face and four side planes, all counted active')
    check(planes[0]==(0.,0.,1.,10.) and all(p.uint(at+16)&255==1 for at in range(begin,end,20)),
          'input face copied verbatim and every generated plane enabled')
    objects=[f.add('dynamic',sphere) for sphere in ((0.,0.,20.,1.),(0.,0.,5.,1.),(8.,0.,20.,1.))]
    # add() updates the graph; refresh only the declared camera cache afterward.
    p.put_floats(occluder+0x118,p.floats(f.native_camera+0x74,3));p.mu.mem_write(occluder+0x1b5,b'\0')
    check(f.draw()==1,'whole original Scene executes nonempty Occlusion consumer with cached-input boundary')
    drawn=[e[1] for e in f.events if e[0]=='node-draw']
    check(drawn==[objects[1][1],objects[2][1]],'behind-inside object hidden; foreground and side objects retained')
    stamp=p.uint(f.scene_object+0x40)
    check(p.uint(objects[0][1]+0x64)==(stamp-1)&0xffffffff and
          all(p.uint(s+0x64)==stamp for o,s in objects[1:]),'occluded support mark rolls back to previous frame')
    f.call(0x421640,this=occluder,args=(0,0))
    check(f.draw()==1 and [e[1] for e in f.events if e[0]=='node-draw']==drawn,
          'disabled Occlusion node still participates in original visibility/Scene occlusion')
    old_planes=bytes(p.mu.mem_read(p.uint(occluder+0x16c),100))
    f.call(0x46f880,this=occluder)
    check(f.words(occluder,0xc8,0xd4)==[0]*3 and not(p.uint(occluder+0x1b4)&255),
          'actual geometry clear deletes owned buffers and clears initialized flag')
    check(bytes(p.mu.mem_read(p.uint(occluder+0x16c),100))==old_planes,
          'geometry clear does not erase cached occlusion planes')
    check(f.draw()==1 and [e[1] for e in f.events if e[0]=='node-draw']==drawn,
          'Scene ignores failed uninitialized plane build and still consumes previous cached planes')
    f.close()


def plane_boundaries():
    f=OcclusionFixture();p=f.p
    inputs=[([],[(0.,0.,1.,10.)],0),
            ([(-2.,-2.,10.),(-2.,2.,10.),(2.,2.,10.)],[],2),
            ([(-2.,-2.,10.),(-2.,0.,10.),(-2.,2.,10.),(2.,2.,10.),
              (2.,-2.,10.),(-2.,-2.,10.)],[(0.,0.,1.,10.)],5),
            (None,[(0.,0.,1.,10.),(0.,0.,1.,10.)],6)]
    for border,faces,count in inputs:
        obj=f.prepared_occluder();f.cache_silhouette(obj,border,faces)
        check(f.call(0x4702e0,this=obj,args=(f.native_camera,))&255==1,
              'original cached plane boundary completes')
        check(p.uint(obj+0x170)-p.uint(obj+0x16c)==20*count and p.uint(obj+0x178)==count,
              'border gate / explicit closing point / adjacent side dedup / face duplicates')
    f.close()


CASES={'lifetime':lifetime_and_world,'weld':weld,'scene':cached_planes_and_scene,'planes':plane_boundaries}


def main(case):
    CASES[case]()
    print(f'PASS {checks}/{checks}: Occlusion {case}')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    for case in sys.argv[1:] or CASES:
        result=run_bounded(Path(__file__),(case,))
        if result:raise SystemExit(result)
