#!/usr/bin/env python3
"""Actual BSP/Octree nonempty static and occlusion registration consumers.

Explicit decoded tree/sphere/Zone inputs; original factories, insertion,
reciprocal drains and destruction. No full Occlusion Init/Scene/render claim.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_bsp_runtime import BspFixture,pointers
from probe_pc_octree_runtime import OctreeFixture

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def setup(kind):
    f=BspFixture() if kind=='bsp' else OctreeFixture();p=f.p
    root,children=f.bsp() if kind=='bsp' else f.octree()
    zone=p.uint(f.root+0x60);refs=p.uint(zone+8)&65535
    p.mu.mem_write(zone+8,(refs+1).to_bytes(2,'little'));p.put_uint(root+0x60,zone)
    f.call(0x4259e0,this=root,args=(f.scene_object,))
    return f,root,children,zone


def static_registration(kind):
    f,root,children,zone=setup(kind);p=f.p
    cases=(((4.,0.,0.,1.),[0]),((-4.,0.,0.,1.),[1]),((1.,0.,0.,1.),[0,1])) if kind=='bsp' else (
          ((4.,4.,4.,1.),[7]),((0.,4.,4.,1.),[6,7]),((.7,.7,.9,1.),[4,5,6,7]))
    objects=[]
    for zoned in (False,True):
        p.put_uint(root+0x60,zone if zoned else 0)
        for sphere,indices in cases:
            obj=f.object(0x41a7c0);objects.append(obj);p.put_floats(obj+0x38,sphere)
            expected=[root] if kind=='bsp' and zoned and len(indices)==2 else [children[i] for i in indices]
            f.call(0x480ad0 if kind=='bsp' else 0x449a90,this=root,args=(obj,))
            f.objects.remove(obj) # native intrusive references now own the object
            check(all(pointers(p,node,0x30).count(obj)==int(node in expected) for node in [root,*children]),
                  'BSP Zoned touching static stays root; Octree always uses its sphere mask')
            check(p.uint(obj+8)&65535==len(expected),'one actual intrusive reference per membership')
            check(p.uint(obj+0x88)==f.scene_object,'leaf append publishes inherited Scene80 into Static88')
    # Repeat the last insertion: no dedup, each occurrence retains one reference.
    obj=objects[-1]
    f.call(0x480ad0 if kind=='bsp' else 0x449a90,this=root,args=(obj,))
    check(all(pointers(p,node,0x30).count(obj)==2*int(node in expected) for node in [root,*children]),
          'duplicate static registration appends again, no dedup')
    check(p.uint(obj+8)&65535==2*len(expected),'duplicate membership retains independently')
    p.put_uint(root+0x60,zone);f.close()
    check(all(obj in f.freed for obj in objects),'all static refs released by actual recursive graph destruction')


def occlusion_registration(kind):
    f,root,children,zone=setup(kind);p=f.p
    p.put_uint(0x760640,0x43d24430);p.put_uint(0x760688,0x75dd88)
    obj=f.object(0x470a70)
    if kind=='bsp':
        far=(4.,0.,0.,1.);touch=(1.,0.,0.,1.);far_indices=[0];touch_indices=[0,1]
    else:
        far=(4.,4.,4.,1.);touch=(0.,4.,4.,1.);far_indices=[7];touch_indices=[6,7]
    cases=[(far,0,True,[children[i] for i in far_indices]),
           (touch,0,True,[root]),(touch,0x400,True,[children[i] for i in touch_indices]),
           (touch,0x100400,True,[children[i] for i in touch_indices]),
           (touch,0,False,[children[i] for i in touch_indices]),
           (touch,0,True,[root])]
    for sphere,flags,zoned,expected in cases:
        f.call(0x46dcf0,this=obj,args=(1,));p.put_floats(obj+0x1a4,sphere)
        p.put_uint(obj+0xb0,flags) # Enabled200 deliberately clear: insertion does not gate it.
        p.put_uint(root+0x60,zone if zoned else 0)
        f.call(0x480630 if kind=='bsp' else 0x449d30,this=root,args=(obj,))
        check(pointers(p,obj,0xb4)==expected,'actual Occlusion reverse membership, no billboard exception')
        check(all(pointers(p,node,0x40)==([obj] if node in expected else []) for node in [root,*children]),
              'all forward Occlusion membership lists agree')
        check(p.uint(obj+8)&65535==0,'Occlusion memberships borrowed, unlike Static owning refs')
    f.call(0x46dcf0,this=obj,args=(1,))
    check(pointers(p,obj,0xb4)==[] and all(pointers(p,node,0x40)==[] for node in [root,*children]),
          'actual notifying drain clears both directions')
    p.put_uint(root+0x60,zone);f.close();check(obj in f.freed,'original empty geometry destructor remains valid')


def debug_static_scene(kind):
    f=BspFixture() if kind=='bsp' else OctreeFixture();p=f.p
    root,children=f.bsp() if kind=='bsp' else f.octree()
    if kind=='octree':
        p.put_floats(root+0x84,(0.,0.,20.));p.put_floats(root+0xb0,(-100.,-100.,0.,100.,100.,100.))
    old=f.root;system=p.uint(f.scene_object+0x38);zone=p.uint(old+0x60)
    # Explicit decoded graph replacement transfers the existing Zone ref.
    p.put_uint(old+0x60,0);p.put_uint(root+0x60,zone)
    p.put_uint(p.uint(zone+0xb8),root);p.put_uint(system+0x1d4,root)
    p.put_uint(root+0x74,system);f.objects.remove(root);f.root=root
    f.call(0x4259e0,this=root,args=(f.scene_object,));f.call(0x426890,this=old,args=(1,))
    obj=f.object(0x41a7c0);p.put_floats(obj+0x38,(0.,4.,30.,1.))
    expected=[root] if kind=='bsp' else [children[6],children[7]]
    for _ in range(2):f.call(0x480ad0 if kind=='bsp' else 0x449a90,this=root,args=(obj,))
    f.objects.remove(obj)
    check(p.uint(obj+8)&65535==2*len(expected) and
          all(pointers(p,node,0x30).count(obj)==2*int(node in expected) for node in [root,*children]),
          'actual duplicated Static memberships before Scene traversal')
    debug=p.uint(0x75526c)
    if not debug:
        # Static-only setup did not trigger the ordinary lazy Debug factory.
        # Execute that original factory and publish it as original callers do.
        debug=f.call(0x41e380);p.put_uint(0x75526c,debug)
    p.mu.mem_write(debug+0x21,b'\1')
    for _ in range(2):
        check(f.draw()==1 and len([e for e in f.events if e[0]=='static-draw'])==1,
              'whole original DEBUG unclipped traversal draws duplicated Static support once per frame')
        check(p.uint(obj+0x78)==p.uint(f.scene_object+0x40),'Static support mark agrees with actual frame stamp')
    p.mu.mem_write(debug+0x21,b'\0');f.close()
    check(obj in f.freed,'actual Scene destruction releases duplicate Static references')


def main(kind,consumer):
    {'static':static_registration,'occlusion':occlusion_registration,'debug-scene':debug_static_scene}[consumer](kind)
    print(f'PASS {checks}/{checks}: PC {kind} {consumer} consumer');return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    for kind,consumer in ((a,b) for a in ('bsp','octree') for b in ('static','occlusion','debug-scene')):
        result=run_bounded(Path(__file__),(kind,consumer))
        if result:raise SystemExit(result)
