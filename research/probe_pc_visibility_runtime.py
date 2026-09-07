#!/usr/bin/env python3
"""Original PC Visibility traversal/culling, explicit manager-construction gap.

Uses VisibilityFixture's documented borrowed manager record. All processing,
plane allocation, native spatial/model/camera objects and teardown are real
PC instructions. No Occlusion/portal/GPU completeness claim is made here.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_partition_init import VisibilityFixture

checks = 0


def check(value,label):
    global checks
    checks += 1
    if not value: raise AssertionError(label)


def pointers(p,obj,offset):
    begin,end = p.uint(obj+offset+4),p.uint(obj+offset+8)
    check(0 <= end-begin <= 128 and (end-begin)%4==0,'bounded visibility pointer vector')
    return [p.uint(at) for at in range(begin,end,4)]


class CullingFixture(VisibilityFixture):
    def __init__(self):
        super().__init__()
        self.scene_object = self.initialized_scene()
        self.root = self.p.uint(self.p.uint(self.scene_object+0x38)+0x1d4)
        self.native_camera = self.object(0x4a9120)
        self.p.mu.mem_write(self.native_camera+0x230,b'\1')
        self.call(0x428af0,this=self.native_camera,args=(1,))
        self.entry_planes = []
        p = self.p

        def observe(mu,address,size,_):
            if address == 0x46c4d0:
                record = p.uint(self.visibility+0x44)+p.uint(self.visibility+0x50)*20
                begin,end = p.uint(record+4),p.uint(record+8)
                check(0<=end-begin<=20*16 and (end-begin)%20==0,'bounded original active plane stack')
                self.entry_planes.append([(p.floats(at,4),p.uint(at+16)&255)
                                          for at in range(begin,end,20)])
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)

    def add(self,kind,sphere):
        p=self.p
        if kind=='dynamic':
            obj=self.node()
            offset=0xb4
        else:
            obj=self.object(0x41a7c0 if kind=='static' else 0x4cd950)
            offset=0x14 if kind=='static' else 0x10
        model,mesh=self.model(),self.mesh_record(sphere)
        self.call(0x479e20,this=model,args=(mesh,))
        self.append(obj,offset,model)
        if kind=='dynamic':
            self.call(0x421a60,this=self.p.uint(self.scene_object+0x14),args=(obj,))
            self.nodes.remove(obj)
            self.call(0x45a7d0,this=p.uint(0x75db90))
        elif kind=='static':
            self.call(0x426740,this=self.root,args=(obj,))
            self.objects.remove(obj)
        else:
            check(p.uint(self.root+0x78)==0,'only one owned partition payload in fixture')
            p.put_uint(self.root+0x78,obj) # explicit decoded graph relationship
            self.objects.remove(obj)
            self.call(0x4259e0,this=self.root,args=(self.scene_object,))
        return obj,obj+offset

    def build(self,camera=None):
        self.entry_planes.clear()
        self.call(0x46d270,this=self.visibility,
                  args=(self.scene_object,camera or self.native_camera))
        return pointers(self.p,self.visibility,0x28)


def mixed_supports_and_flags():
    f=CullingFixture()
    p=f.p
    payload,ps=f.add('partition',(0.,0.,10.,1.))
    inside,ss=f.add('static',(0.,0.,20.,1.))
    outside,os=f.add('static',(100.,0.,10.,1.))
    node,ns=f.add('dynamic',(0.,0.,30.,1.))
    far,fs=f.add('dynamic',(100.,0.,10.,1.))
    disabled,ds=f.add('dynamic',(0.,0.,40.,1.))
    # Keep borrowed membership while testing the consumer's own flag gate;
    # this is not a claim that native SetEnabled leaves registrations intact.
    p.put_uint(disabled+0xb0,p.uint(disabled+0xb0)&~0x200)
    f.call(0x426740,this=f.root,args=(inside,))
    f.call(0x426690,this=f.root,args=(node,))
    check(f.build()==[ps,ss,ns],
          'original traversal emits payload/static/dynamic order, culls far, skips disabled, deduplicates')
    check(p.uint(f.visibility+0x14)==p.uint(f.scene_object+0x40)==p.uint(f.root+0x7c)==1,
          'manager14 increments and stamps Scene40/PartitionNode7C')
    check(p.uint(payload+0x74)==p.uint(inside+0x78)==p.uint(node+0x118)==1 and
          p.uint(outside+0x78)==p.uint(far+0x118)==p.uint(disabled+0x118)==0,
          'only submitted supports receive current mark64 at their adjusted offsets')
    check(pointers(p,f.visibility,0x18)==[], 'no occlusion objects fabricated for visibility vector18')
    planes=f.entry_planes[0]
    check(len(planes)==6 and all(flag==1 for _,flag in planes),
          'actual build creates six enabled clip planes')
    check(planes[0][0]==(0.,0.,1.,0.) and
          p.floats(f.native_camera+0x1c4,4)==(0.,0.,1.,1.),
          'visibility first plane passes through camera, differs from camera near=1 plane')
    check(f.build()==[ps,ss,ns] and p.uint(f.scene_object+0x40)==2,
          'second frame clears logical outputs and refreshes stamps without duplicate accumulation')
    p.mu.mem_write(far+0x130,b'\1')
    check(f.build()==[ps,ss,ns,fs], 'RenderNode bypass130 skips sphere cull but not Enabled gate')
    p.mu.mem_write(f.visibility+0x3c,b'\0')
    check(f.build()==[ps,ss,os,ns,fs],
          'manager3C disables sphere rejection for all supports, retains dynamic Enabled gate')
    p.mu.mem_write(p.uint(0x75526c)+0x21,b'\1')
    check(f.build()==[ps,ss,os,ns,fs,ds] and not f.entry_planes,
          'Debug21 selects unclipped46B870; includes disabled member, which later draw may reject')
    p.mu.mem_write(p.uint(0x75526c)+0x21,b'\0')
    f.close()


def near_plane_override_and_wrap():
    f=CullingFixture()
    p=f.p
    near,ns=f.add('dynamic',(0.,0.,.25,.125))
    behind,bs=f.add('static',(0.,0.,-2.,.25))
    check(f.build()==[ns],
          'positive-near sphere accepted before GPU clip; behind-camera sphere rejected')
    override=f.object(0x4a9120)
    p.put_floats(override+0x20,(0.,0.,-10.))
    p.mu.mem_write(override+0x230,b'\1')
    f.call(0x428af0,this=override,args=(1,))
    p.put_uint(f.visibility+0x38,override)
    check(f.build()==[bs,ns] and f.entry_planes[0][0][0]==(0.,0.,1.,-10.),
          'borrowed override38 replaces culling camera and first-plane offset')
    p.put_uint(f.visibility+0x38,0)
    p.put_uint(f.visibility+0x14,0xffffffff)
    p.put_uint(near+0x118,0)
    p.put_uint(behind+0x78,0)
    check(f.build()==[] and p.uint(f.scene_object+0x40)==0,
          'unsigned frame stamp wraps to zero; zero-mark objects are skipped for wrapped frame')
    check(f.build()==[ns] and p.uint(f.scene_object+0x40)==1,
          'following stamp1 resumes ordinary visibility')
    f.close()


def multiple_zone_roots_debug_difference():
    f=CullingFixture()
    p=f.p
    first,fs=f.add('static',(0.,0.,10.,1.))
    root=f.object(0x426910)
    second=f.object(0x41a7c0)
    model,mesh=f.model(),f.mesh_record((0.,0.,20.,1.))
    f.call(0x479e20,this=model,args=(mesh,))
    f.append(second,0x14,model)
    f.call(0x426740,this=root,args=(second,))
    f.objects.remove(second)
    f.call(0x4259e0,this=root,args=(f.scene_object,))
    f.call(0x481080,this=p.uint(f.root+0x60),args=(root,))
    check(f.build()==[fs,second+0x14] and len(f.entry_planes)==2,
          'normal path iterates both borrowed Zone roots')
    p.mu.mem_write(p.uint(0x75526c)+0x21,b'\1')
    check(f.build()==[fs] and p.uint(root+0x7c)==1,
          'Debug21 native loop repeats selected system root, not each Zone root')
    p.mu.mem_write(p.uint(0x75526c)+0x21,b'\0')
    f.close()


def main():
    mixed_supports_and_flags()
    near_plane_override_and_wrap()
    multiple_zone_roots_debug_difference()
    print(f'PASS {checks}/{checks}: original PC Visibility processing with explicit constructor gap')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
