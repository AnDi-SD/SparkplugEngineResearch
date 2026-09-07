#!/usr/bin/env python3
"""Actual PC portal lifetime/geometry/borrowed node vector and Scene traversal.

Graph relationships are explicit decoded inputs, not a full serializer claim.
Visibility constructor gap and device boundaries are inherited unchanged.
Every native call remains bounded at100k instructions/2sec; child30sec.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_partition_init import VisibilityFixture
from probe_pc_scene_render_runtime import SceneRenderFixture

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def register(p):
    for record,identity,parent in ((0x7614b8,0x6523ac37,0x7555f8),(0x761518,0xabb5ab2c,0x75dd88)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)


def vector(p,obj):
    begin,end=p.uint(obj+0xb8),p.uint(obj+0xbc)
    assert 0<=end-begin<=128 and (end-begin)%4==0
    return [p.uint(a) for a in range(begin,end,4)]


QUAD=(-2.,-2.,10.,2.,-2.,10.,2.,2.,10.,-2.,2.,10.)


def lifetime():
    f=VisibilityFixture();p=f.p;register(p)
    portal=f.object(0x481370);node=f.object(0x481810);zone=f.object(0x480fd0)
    check(f.allocations[portal]==0x38 and p.uint(portal)==0x6ebb10,'exact38 primary8')
    check(f.allocations[node]==0xc4 and p.uint(node)==0x6ebb44,'exactC4 primary14')
    check(f.words(portal,0x14,0x20)==[0,0,0] and p.uint(portal+0x20)==0xcccccc01 and
          f.words(portal,0x24,0x34)==[0xcccccccc]*4 and p.uint(portal+0x34)==0,
          'empty destination/geometry, Open1, untouched plane/padding, stamp0')
    check(p.uint(node+0xb4)==0xcccccccc and vector(p,node)==[],
          'borrowed vector empty and allocator word untouched')
    check(f.call(0x408370,this=portal,args=(0x44de07fd,))&255==1 and
          f.call(0x408370,this=portal,args=(0x695c0f65,))&255==0,'portal is Named, not Node')
    check(f.call(0x408370,this=node,args=(0x695c0f65,))&255==1,'portal node derives Node')
    points=p.allocate(48);p.put_floats(points,QUAD)
    f.call(0x481130,this=portal,args=(4,points));geometry=p.uint(portal+0x1c)
    p.put_uint(portal+0x14,zone);p.mu.mem_write(portal+0x20,b'\0');p.put_uint(portal+0x34,123)
    for _ in range(3):f.call(0x481930,this=node,args=(portal,))
    check(vector(p,node)==[portal]*3 and p.uint(portal+8)&65535==0,
          'actual append preserves duplicates, no portal retain')
    # Reader refuses null, but the lower-level raw container accepts it.
    f.call(0x481930,this=node,args=(0,))
    check(vector(p,node)==[portal]*3+[0],'native append itself has no null guard')
    f.call(0x421640,this=node,args=(0,0))
    check(p.uint(portal+0x20)&255==0,'Node Enabled setter does not alter portal Open')
    p.mu.mem_write(portal+0x20,b'\1');f.call(0x421640,this=node,args=(1,0))
    check(p.uint(portal+0x20)&255==1,'Node Enabled remains independent of portal Open')
    p.put_floats(node+0x20,(80.,90.,100.));f.call(0x421420,this=node,args=(1,))
    check(p.floats(geometry,12)==QUAD and p.floats(portal+0x24,4)==(0.,0.,1.,10.),
          'actual inherited Node world does not transform portal polygon/plane')
    for source in (portal,node):
        clone=f.call(p.uint(p.uint(source)+8),this=source);f.objects.append(clone)
        if source==portal:
            check(f.words(clone,0x14,0x20)==[0,0,0] and p.uint(clone+0x20)&255==1 and
                  f.words(clone,0x24,0x34)==[0xcccccccc]*4 and p.uint(clone+0x34)==0,
                  'Named-only clone drops destination/polygon/Open/stamp')
        else:
            check(vector(p,clone)==[] and p.floats(clone+0x20,3)==(80.,90.,100.),
                  'Node-only clone retains base transform, drops portal vector')
    f.call(0x4815a0,this=node,args=(1,));f.objects.remove(node)
    check(portal not in f.freed,'node destruction frees vector only, no portal release')
    f.call(0x481110,this=portal,args=(1,));f.objects.remove(portal)
    check(geometry in f.freed and zone not in f.freed,'portal owns geometry, destination is borrowed')
    f.close();check(True,'all tracked allocations released exactly once')


def geometry():
    f=VisibilityFixture();p=f.p;register(p);portal=f.object(0x481370)
    source=p.allocate(96);old=0
    polygons=[QUAD,QUAD[:9],QUAD[:9]+(-20.,-20.,90.),
              (0.,0.,1.,1.,0.,1.,0.,1.,1.,.2,.2,1.,0.,.5,1.),
              tuple(v for point in reversed([QUAD[i:i+3] for i in range(0,12,3)]) for v in point),
              (1.,2.,3.,2.,4.,6.,3.,6.,9.)]
    for index,polygon in enumerate(polygons):
        p.put_floats(source,polygon);before=bytes(p.mu.mem_read(source,len(polygon)*4))
        f.call(0x481130,this=portal,args=(len(polygon)//3,source))
        owned=p.uint(portal+0x1c)
        check(owned!=source and f.allocations[owned]==len(polygon)*4 and
              bytes(p.mu.mem_read(owned,len(polygon)*4))==before,'exact deep polygon copy')
        check(not old or old in f.freed,'reinit frees previous owned polygon')
        expected=(0.,0.,1.,10.) if index<3 else (0.,0.,1.,1.) if index==3 else (0.,0.,-1.,-10.) if index==4 else (0.,0.,0.,0.)
        check(p.floats(portal+0x24,4)==expected,
              'plane only first three points; nonplanar/concave tail not validated; degenerate plane zero')
        check(bytes(p.mu.mem_read(source,len(polygon)*4))==before,'borrowed input unchanged')
        old=owned
    f.close();check(old in f.freed,'last polygon released by original destructor')


class PortalSceneFixture(SceneRenderFixture):
    def __init__(self):
        super().__init__();p=self.p;register(p)
        # Original clipping lazily constructs global7629B0 and registers its
        # dtor with imported MSVCRT __dllonexit. Explicit record-only CRT
        # boundary; no native geometry helper is replaced or OS called.
        self.exit_callbacks=[]
        assert bytes(p.mu.mem_read(0x60dfac,2))==b'\xff\x25'
        p.put_uint(p.uint(0x60dfae),self.RX_VIS+0x30)
        def onexit(p):
            callback,begin,end=(p.uint(p.reg('ESP')+4*i) for i in (1,2,3))
            assert (callback,begin,end)==(0x6d7f90,0x84d7b4,0x84d7b0)
            assert not self.exit_callbacks
            self.exit_callbacks.append(callback)
            p.fixture_return(eax=callback) # cdecl success is callback pointer
        p.seams[self.RX_VIS+0x30]=onexit
        self.portal=self.object(0x481370);self.destination=self.object(0x480fd0)
        self.other_root=self.object(0x426910)
        p.put_uint(self.portal+0x14,self.destination) # explicit borrowed decoded relation
        self.call(0x481080,this=self.destination,args=(self.other_root,))
        p.put_uint(self.other_root+0x80,self.scene_object)
        self.points=p.allocate(96);p.put_floats(self.points,QUAD)
        self.call(0x481130,this=self.portal,args=(4,self.points))
        array=self.buffer((0.,));p.put_uint(array,self.portal)
        p.put_uint(self.root+0x68,array);p.put_uint(self.root+0x6c,array+4);p.put_uint(self.root+0x70,array+4)
        p.mu.mem_write(self.portal+8,(1).to_bytes(2,'little'));self.objects.remove(self.portal)
        self.entries=[]
        def observe(mu,address,size,_):
            if address==0x46c4d0:
                selected=p.uint(p.reg('ESP')+4)
                record=p.uint(self.visibility+0x44)+20*p.uint(self.visibility+0x50)
                begin,end=p.uint(record+4),p.uint(record+8)
                assert 0<=end-begin<=400 and (end-begin)%20==0
                self.entries.append((selected,[(p.floats(a,4),p.uint(a+16)&255) for a in range(begin,end,20)]))
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)

    def remote(self,sphere):
        obj,support=self.add('dynamic',sphere)
        self.call(0x424dd0,this=obj,args=(1,))
        self.call(0x426690,this=self.other_root,args=(obj,))
        return obj,support

    def render(self):
        self.entries.clear();result=self.draw()
        check(result==1,'actual whole Scene draw succeeds')
        return [event[1] for event in self.events if event[0]=='node-draw']

    def close(self):
        for callback in reversed(self.exit_callbacks):self.call(callback)
        self.exit_callbacks.clear()
        # Actual scratch dtor above runs before Visibility's pool shutdown.
        super().close()


def scene_gates():
    f=PortalSceneFixture();p=f.p
    node,support=f.remote((0.,0.,20.,1.))
    check(f.render()==[support] and len(f.entries)==2,'open front-facing portal traverses remote Zone')
    check(p.uint(f.portal+0x34)==p.uint(f.other_root+0x7c)==p.uint(f.scene_object+0x40),
          'portal and destination root marked with current stamp')
    planes=f.entries[1][1]
    check(len(planes)==6 and all(flag for _,flag in planes) and
          planes[:2]==f.entries[0][1][:2],'new aperture frustum keeps first two camera planes')
    check(abs(planes[2][0][1]-.9805806875)<1e-7 and abs(planes[2][0][2]-.1961161345)<1e-7,
          'actual aperture edge normal built from camera and clipped polygon')
    p.mu.mem_write(f.portal+0x20,b'\0');stamp=p.uint(f.portal+0x34)
    check(f.render()==[] and len(f.entries)==1 and p.uint(f.portal+0x34)==stamp,
          'closed portal skipped BEFORE marking, destination not traversed')
    p.mu.mem_write(f.portal+0x20,b'\1')
    p.put_floats(f.points,tuple(v for point in reversed([QUAD[i:i+3] for i in range(0,12,3)]) for v in point))
    f.call(0x481130,this=f.portal,args=(4,f.points))
    check(f.render()==[] and len(f.entries)==1 and p.uint(f.portal+0x34)==p.uint(f.scene_object+0x40),
          'back-facing portal skipped AFTER marking')
    f.close()


def scene_aperture():
    f=PortalSceneFixture();p=f.p
    inside,isupport=f.remote((0.,0.,20.,.25))
    outside,osupport=f.remote((8.,0.,20.,.25))
    # This remote-zone member is in front of the portal but still inside its
    # cone: the aperture uses camera near/far, NOT portal plane as new near.
    before,bsupport=f.remote((0.,0.,5.,.25))
    check(f.render()==[isupport,bsupport],'aperture cone rejects side sphere, not pre-portal sphere')
    polygon=tuple(v+(100. if axis==0 else 0.) for index,v in enumerate(QUAD) for axis in [index%3])
    p.put_floats(f.points,polygon);f.call(0x481130,this=f.portal,args=(4,f.points))
    check(f.render()==[] and len(f.entries)==1,'fully clipped aperture prevents destination traversal')
    # An aperture crossing the camera's right side is clipped, not discarded.
    shifted=tuple(v+(6. if index%3==0 else 0.) for index,v in enumerate(QUAD))
    p.put_floats(f.points,shifted);f.call(0x481130,this=f.portal,args=(4,f.points))
    check(f.render()==[osupport] and len(f.entries)==2,'partially clipped aperture keeps visible remote support')
    check(len(f.entries[1][1])==6,'clipped quadrilateral still generates four edge planes')
    check(f.exit_callbacks==[0x6d7f90],'actual lazy scratch registers one original cleanup callback')
    f.close()


def scene_cycle():
    f=PortalSceneFixture();p=f.p
    node,support=f.remote((0.,0.,20.,.25))
    back=f.object(0x481370);p.put_uint(back+0x14,p.uint(f.root+0x60))
    f.call(0x481130,this=back,args=(4,f.points))
    array=f.buffer((0.,));p.put_uint(array,back)
    p.put_uint(f.other_root+0x68,array);p.put_uint(f.other_root+0x6c,array+4);p.put_uint(f.other_root+0x70,array+4)
    p.mu.mem_write(back+8,(1).to_bytes(2,'little'));f.objects.remove(back)
    # Deliberately same-facing synthetic pair, NOT the stock reverse-oriented
    # geometry. A->B->A must terminate through per-portal marks, not root gate.
    check(f.render()==[support] and [root for root,_ in f.entries]==[f.root,f.other_root,f.root],
          'actual cyclic two-Zone graph revisits root once, support emitted only once')
    check(p.uint(back+0x34)==p.uint(f.portal+0x34)==p.uint(f.scene_object+0x40),
          'both directed portals marked, next attempt skips already visited portal')
    check(f.render()==[support] and len(f.entries)==3,'next frame refreshes same cycle without accumulation')
    f.close()


CASES={'lifetime':lifetime,'geometry':geometry,'gates':scene_gates,'aperture':scene_aperture,'cycle':scene_cycle}


def main(case):
    CASES[case]();print(f'PASS {checks}/{checks}: PC ZonePortal {case}');return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    for case in sys.argv[1:] or CASES:
        result=run_bounded(Path(__file__),(case,))
        if result:raise SystemExit(result)
