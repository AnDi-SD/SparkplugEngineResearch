#!/usr/bin/env python3
"""Bounded emulated PC spNode/world/matrix fixtures; no game launch or assets.

Original protected bridges run unchanged. Collision and scene-manager work
is absent in transform fixtures; their full integration is not claimed.
"""
from __future__ import annotations
import math
from pathlib import Path
import random
import sys
from pc_instruction_emulator import PcInstructions, run_bounded

IDENTITY = (1,0,0,0,1,0,0,0,1)
checks = 0
def check(value, description):
    global checks
    checks += 1
    if not value:
        raise AssertionError(description)

def near(a,b):
    return all(math.isclose(x,y,rel_tol=2e-5,abs_tol=2e-5) for x,y in zip(a,b))

def mul(a,b):
    return tuple(sum(a[3*r+k]*b[3*k+c] for k in range(3)) for r in range(3) for c in range(3))

def vector(v,m):
    return tuple(sum(v[k]*m[3*k+c] for k in range(3)) for c in range(3))

def cross(a,b):
    return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])

def normalize(a):
    length=math.sqrt(sum(x*x for x in a))
    return tuple(x/length for x in a) if length>.001 else (0,0,0)

def billboard(camera, axis):
    if camera is None:
        return IDENTITY
    forward=normalize(tuple(-x for x in camera[6:9]))
    up=(0,1,0)
    parallel=abs(abs(sum(x*y for x,y in zip(up,forward)))-1)<.001
    if parallel:
        if axis==1:
            forward=tuple(-x for x in normalize(camera[3:6]))
        else:
            up=normalize(camera[3:6])
    right=normalize(cross(up,forward))
    if axis==1:
        forward=normalize(cross(right,up))
    else:
        up=normalize(cross(forward,right))
    return (*cross(up,forward),*up,*forward)

def node(p, position=(0,0,0), scale=(1,1,1), orientation=IDENTITY, flags=0x70a01):
    n=p.allocate(0xb4)
    head=p.allocate(12)
    p.put_uint(n+0x18,head)
    p.put_uint(head,head)
    p.put_uint(head+4,head)
    p.put_uint(n+0xb0,flags)
    p.put_floats(n+0x20,position)
    p.put_floats(n+0x30,scale)
    p.put_floats(n+0x40,orientation)
    p.put_floats(n+0x74,(91,92,93))
    p.put_floats(n+0x80,(94,95,96))
    p.put_floats(n+0x8c,IDENTITY)
    return n

def guest():
    p=PcInstructions()
    n=node(p)
    p.run(0x421420,n,[0],stop_at=0x42142e)
    check(p.uint(0x13b1510)==0x442fa6 and p.reg('EBX')==0x70a01,
          'world protected bridge resolves flags read')
    q=p.allocate(16)
    p.put_floats(q,(0,0,1,0))
    p.put_uint(n+0xb0,0x70a00)
    p.run(0x420640,n,[q])
    check(p.uint(0x13b13cc)==0x4023a9 and p.uint(n+0xb0)==0x70a01,
          'quaternion protected bridge sets dirty flag')
    check(near(p.floats(n+0x40,9),(-1,0,0,0,-1,0,0,0,1)), 'full quaternion setter matrix')
    out=p.allocate(64)
    p.run(0x461d70,out,[n+0x74,n+0x8c,n+0x80])
    check(p.uint(0x13b162c)==0x4061e8, 'matrix protected stack preamble')
    check(near(p.floats(out,16),(94,0,0,0,0,95,0,0,0,0,96,0,91,92,93,1)),
          'complete cached PRS matrix builder')

    rng=random.Random(0x421420)
    for trial in range(32):
        p.reset_arena()
        a=tuple(rng.uniform(-2,2) for _ in range(9))
        b=tuple(rng.uniform(-2,2) for _ in range(9))
        pp=tuple(rng.uniform(-10,10) for _ in range(3))
        lp=tuple(rng.uniform(-5,5) for _ in range(3))
        ps=tuple(rng.uniform(.25,3) for _ in range(3))
        ls=tuple(rng.uniform(.25,3) for _ in range(3))
        parent=node(p,pp,ps,a)
        p.run(0x421420,parent,[0])
        check(near(p.floats(parent+0x74,3),pp) and near(p.floats(parent+0x80,3),ps)
              and near(p.floats(parent+0x8c,9),a), 'root copies local PRS')
        mask=(trial % 8)<<16
        child=node(p,lp,ls,b,flags=mask|7)
        p.put_uint(child+0x2c,parent)
        p.run(0x421420,child,[0])
        local_scaled=tuple(lp[i]*ps[i] for i in range(3)) if mask&0x40000 else lp
        translated=tuple(x+y for x,y in zip(vector(local_scaled,a),pp))
        expected_pos=translated if mask&0x10000 else (91,92,93)
        expected_scale=tuple(x*y for x,y in zip(ls,ps)) if mask&0x40000 else ls
        expected_orientation=mul(b,a) if mask&0x20000 else b
        check(near(p.floats(child+0x74,3),expected_pos) and
              near(p.floats(child+0x80,3),expected_scale) and
              near(p.floats(child+0x8c,9),expected_orientation) and
              p.uint(child+0xb0)==mask, 'all inheritance masks and low-bit cleanup')
        out=p.allocate(64)
        p.run(0x461d70,out,[child+0x74,child+0x8c,child+0x80])
        affine=tuple(expected_scale[r]*expected_orientation[r*3+c] if r<3 and c<3
                     else expected_pos[c] if r==3 and c<3 else 1 if r==c==3 else 0
                     for r in range(4) for c in range(4))
        check(near(p.floats(out,16),affine), 'scaled orientation then affine translation')

    # Actual virtual child recursion, not a substituted callback.
    p.reset_arena()
    root=node(p,(2,3,4),(2,3,4),flags=0x70a03)
    child=node(p,(1,2,3),flags=0x70a00)
    grand=node(p,(1,1,1),flags=0x70a00)
    table=p.allocate(0x38)
    p.put_uint(table+0x30,0x421420)
    for parent,descendant in ((root,child),(child,grand)):
        head=p.uint(parent+0x18)
        link=p.allocate(12)
        p.put_uint(head,link)
        p.put_uint(link,head)
        p.put_uint(link+8,descendant)
        p.put_uint(descendant,table)
        p.put_uint(descendant+0x2c,parent)
    p.run(0x421420,root,[0])
    check(near(p.floats(child+0x74,3),(4,9,16)) and
          near(p.floats(grand+0x74,3),(6,12,20)), 'three-level virtual dirty propagation')
    check(all(p.uint(n+0xb0)==0x70a00 for n in (root,child,grand)), 'whole-tree clears low three bits')
    p.put_floats(root+0x20,(50,60,70))
    p.run(0x421420,root,[0])
    check(near(p.floats(root+0x74,3),(2,3,4)), 'clean update preserves cache')
    p.run(0x421420,root,[1])
    check(near(p.floats(grand+0x74,3),(54,69,86)), 'external inherited dirty bit updates tree')

    # Camera is a synthetic spNode cache, not a scene singleton or renderer.
    cameras=[None,IDENTITY,(1,0,0,0,0,-1,0,1,0),(1,0,0,0,0,1,0,-1,0),
             (0,0,-1,0,1,0,1,0,0),(1,0,0,0,.8,-.6,0,.6,.8)]
    for camera in cameras:
        for axis in (1,2,3):
            p.reset_arena()
            n=node(p,orientation=(0,1,0,-1,0,0,0,0,1),flags=axis<<20)
            c=node(p) if camera else 0
            if camera: p.put_floats(c+0x8c,camera)
            p.run(0x4207e0,n,[c])
            expected=billboard(camera,axis)
            check(near(p.floats(n+0x8c,9),expected),'billboard axis/null/parallel camera')
            engine=p.allocate(0x20)
            p.put_uint(engine+0x1c,c)
            p.put_uint(0x755274,engine)
            p.put_floats(n+0x8c,IDENTITY)
            p.run(0x421420,n,[0])
            check(near(p.floats(n+0x8c,9),expected) and
                  near(p.floats(n+0x74,3),(91,92,93)),
                  'clean billboard still rotates before dirty gate')
            parent=node(p)
            p.put_uint(n+0x2c,parent)
            p.put_uint(n+0xb0,(axis<<20)|1)
            p.run(0x421420,n,[0])
            check(near(p.floats(n+0x8c,9),(0,1,0,-1,0,0,0,0,1)),
                  'parent with orientation inheritance off overwrites billboard')
            p.put_uint(n+0xb0,(axis<<20)|0x20001)
            p.run(0x421420,n,[0])
            check(near(p.floats(n+0x8c,9),expected),
                  'parent with orientation inheritance on preserves billboard')

    # Observe call order only; collision implementation is not substituted
    # with a success claim. Its callback is a named, recording fixture seam.
    p.reset_arena()
    n=node(p,(7,8,9),flags=0x70a07)
    callbacks=p.allocate(8)
    p.put_uint(n+0x68,callbacks)
    p.put_uint(n+0x6c,callbacks+8)
    p.put_uint(callbacks,0x1111)
    p.put_uint(callbacks+4,0x2222)
    observed=[]
    def collision_seam(machine):
        observed.append((machine.reg('ECX'),machine.floats(n+0x74,3),machine.uint(n+0xb0)))
        machine.fixture_return()
    p.seams[0x4651e0]=collision_seam
    p.run(0x421420,n,[0])
    check(observed==[(0x1111,(7,8,9),0x70a07),(0x2222,(7,8,9),0x70a07)],
          'collision callbacks follow cache writes and precede dirty cleanup')
    observed.clear()
    p.run(0x421420,n,[0])
    check(not observed, 'clean non-billboard skips collision callbacks')
    print(f'PASS {checks}/{checks}: protected bridges, root/parent PRS, 8 masks, tree recursion, matrix builder')
    return 0

if __name__=='__main__':
    raise SystemExit(guest() if sys.argv[1:]==['--guest'] else run_bounded(Path(__file__)))
