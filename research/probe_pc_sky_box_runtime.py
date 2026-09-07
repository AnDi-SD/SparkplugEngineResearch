#!/usr/bin/env python3
"""Original PC SkyBox manager lifecycle/snapshot attachment/world/draw dispatch.

Camera draw49E5F0 and renderer deferred flush454850 are recording leaves in
manager dispatch tests. They do not constitute a reconstructed GPU renderer.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_special_registry import SpecialSceneFixture, list_entries

checks = 0
def check(value,label):
    global checks
    checks += 1
    if not value: raise AssertionError(label)

def lifetime_and_initialization():
    f = SpecialSceneFixture()
    p = f.p
    manager = f.call(0x48dc80)
    skies = [f.sky() for _ in range(3)]
    check(f.allocations[manager]==0x24 and p.uint(manager)==0x6ec460,
          'SkyBoxManager actual24 factory/table')
    check(p.uint(manager+0x10)==0xcccccc00 and p.uint(manager+0x14)==0 and
          p.uint(manager+0x18)==0xcccccccc, 'manager disabled, borrowed root0, untouched allocator/padding')
    check(list_entries(p,manager)==[], 'manager constructor owns empty sentinel')
    for sky in (skies[0],skies[1],skies[0],skies[2]):
        f.call(0x48db00,this=manager,args=(sky,))
    check(list_entries(p,manager)==[skies[0],skies[1],skies[0],skies[2]],
          'native skybox append permits duplicate pointers in separate list nodes')
    f.call(0x45a3d0,this=manager,args=(skies[0],))
    check(list_entries(p,manager)==[skies[1],skies[2]], 'sky removal erases all matching pointer entries')
    f.call(0x45a3d0,this=manager,args=(skies[0],))
    check(list_entries(p,manager)==[skies[1],skies[2]], 'missing sky pointer removal no-op')
    root_marker=p.uint(manager+0x1c)
    p.seams[0x412f70]=lambda p:p.fixture_return(8)
    clone=f.call(0x48dce0,this=manager)
    check(list_entries(p,clone)==[] and p.uint(clone+0x14)==0 and
          p.uint(clone+0x10)==0xcccccc00, 'actual blank clone does not copy list/initialized state')
    f.call(0x48dae0,this=clone,args=(1,))
    check(f.call(0x48da20,this=manager)&255==1 and p.uint(manager+0x10)==0xcccccc01,
          'native Init enables manager')
    check(list_entries(p,manager)==[] and p.uint(manager+0x1c)==root_marker,
          'native Init clears borrowed list but retains sentinel')
    check(all(p.uint(sky+8)&65535==0 and sky not in f.freed for sky in skies),
          'list/clone/Init do not own or release SkyBox objects')
    f.call(0x48dae0,this=manager,args=(1,))
    for sky in skies:f.call(0x49e590,this=sky,args=(1,))
    f.close()

def snapshot_attach_and_detach():
    f=SpecialSceneFixture()
    p=f.p
    first,second=f.scene(),f.scene()
    roots=[p.uint(scene+0x14) for scene in (first,second)]
    manager=p.uint(first+0x30)
    skies=[f.sky() for _ in range(3)]
    for sky in skies:f.call(0x421a60,this=roots[0],args=(sky,))
    p.put_uint(manager+0x14,roots[1]) # explicit borrowed target-root member, original setter not claimed
    f.call(0x48db40,this=manager)
    check(list_entries(p,manager)==[] and list_entries(p,p.uint(second+0x30))==skies,
          'snapshot iteration survives own source list removal during scene reparent')
    check(all(p.uint(sky+0x2c)==roots[1] and p.uint(sky+0x3c)==second for sky in skies),
          'original snapshot attach moves actual ownership/scene links')
    manager=p.uint(second+0x30)
    p.put_uint(manager+0x14,roots[1])
    # Explicit external references keep the sky nodes alive after parent release.
    for sky in skies:p.put_uint(sky+8,(p.uint(sky+8)&0xffff0000)|2)
    f.call(0x48da30,this=manager)
    check(list_entries(p,manager)==[] and p.visits[0x402910]>0,
          'protected48DA30 executes402910 detach-until-native-list-empty loop')
    check(all(p.uint(sky+0x2c)==0 and p.uint(sky+0x3c)==0 and p.uint(sky+8)&65535==1
              for sky in skies),'parent unlink clears scene but preserves explicit external reference')
    for sky in skies:
        p.put_uint(sky+8,p.uint(sky+8)&0xffff0000)
        f.call(0x49e590,this=sky,args=(1,))
    f.close()

def world_and_dispatch():
    f=SpecialSceneFixture()
    p=f.p
    scene=f.scene()
    root=p.uint(scene+0x14)
    manager=p.uint(scene+0x30)
    skies=[f.sky() for _ in range(3)]
    for sky in skies:f.call(0x421a60,this=root,args=(sky,))
    root_orientation=(0.,1.,0.,-1.,0.,0.,0.,0.,1.)
    local_orientation=(1.,0.,0.,0.,0.,1.,0.,-1.,0.)
    p.put_floats(root+0x40,root_orientation);p.put_uint(root+0xb0,p.uint(root+0xb0)|1)
    p.put_floats(skies[0]+0x40,local_orientation)
    f.call(0x45a7d0,this=p.uint(0x75db90))
    check(p.floats(skies[0]+0x8c,9)==local_orientation,
          'SkyBox world override copies own local orientation after inherited world update')
    camera=p.allocate(0x238) # recording call argument, not constructed camera
    calls=[]
    results={skies[0]:1,skies[1]:0,skies[2]:3}
    def draw(p):
        obj=p.reg('ECX')
        check(obj in skies and p.uint(p.reg('ESP')+4)==camera,'native sky pass passes each object/camera')
        calls.append(('draw',obj))
        p.fixture_return(4,eax=results[obj])
    def flush(p):
        check(p.reg('ECX')==f.renderer,'native sky pass flush receiver')
        calls.append(('flush',))
        p.fixture_return(eax=0) # ignored by native dispatcher
    p.seams[0x49e5f0]=draw;p.seams[0x454850]=flush
    check(f.call(0x48da60,this=manager,args=(camera,))&255==0,
          'manager combines low bytes even when individual sky returns false')
    check(calls==[('draw',sky) for sky in skies]+[('flush',)],
          'failure does not skip following sky or final renderer flush')
    calls.clear();results[skies[1]]=1
    check(f.call(0x48da60,this=manager,args=(camera,))&255==1,
          'flush return ignored; all light-bit-true draw values yield1')
    check(calls==[('draw',sky) for sky in skies]+[('flush',)],'repeat preserves native order')
    f.close()

def dedicated_sky_draw():
    f=SpecialSceneFixture();p=f.p
    sky=f.sky()
    objects=f.borrowed_renderables(sky,(0x12345678,0x23456789))
    # Actual423D20 releases Fog24, not alpha-sort18. Two borrowed fixture refs
    # plus one external ref allow the native decrements without a fake destructor.
    fog=p.allocate(0x10);p.put_uint(fog+8,3)
    for obj in objects:
        p.mu.mem_write(obj+0x18,b'\1');p.put_uint(obj+0x24,fog)
    camera=p.allocate(0x238)
    calls=[]
    def support_draw(p):
        check(p.reg('ECX')==sky+0xb4 and p.uint(p.reg('ESP')+4)==camera and
              p.uint(p.reg('ESP')+8)==0,'dedicated sky draw passes camera/force0 to BASE support')
        check(all(p.uint(obj+0x24)==0 and bytes(p.mu.mem_read(obj+0x18,1))==b'\1'
                  for obj in objects) and p.uint(fog+8)==1,
              'all sky renderables release fog before base draw, alpha-sort stays unchanged')
        calls.append('base-draw');p.fixture_return(8,eax=0)
    p.seams[0x424b60]=support_draw
    check(f.call(0x49e5f0,this=sky,args=(camera,))&255==0,
          'dedicated sky leaf propagates base support result')
    check(calls==['base-draw'] and p.visits[0x4cc3c0]>0,
          'protected49E5F0 resolves to actual4CC3C0, not no-op derived support')
    # Dedicated function has no outer Enabled gate: mutations precede base gate.
    p.put_uint(sky+0xb0,p.uint(sky+0xb0)&~0x200)
    p.put_uint(fog+8,3)
    for obj in objects:p.put_uint(obj+0x24,fog)
    f.call(0x49e5f0,this=sky,args=(camera,))
    check(all(p.uint(obj+0x24)==0 for obj in objects),
          'disabled sky still drops fog; actual base draw handles Enabled')
    for off in (0xbc,0xc0,0xc4):p.put_uint(sky+off,0)
    f.call(0x49e590,this=sky,args=(1,))
    f.close()

def game_default_camera_binding():
    f=SpecialSceneFixture();p=f.p
    for record,identity,parent in ((0x75e218,0x18df3845,0x75dd88),
                                   (0x763088,0x41672e34,0x75e218)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    scene=f.scene();root=p.uint(scene+0x14);manager=p.uint(scene+0x30)
    camera=f.call(0x4a9120)
    f.call(0x421a60,this=root,args=(camera,))
    skies=[f.sky() for _ in range(3)]
    for sky in skies:f.call(0x421a60,this=root,args=(sky,))
    engine=p.uint(0x755274)
    p.put_uint(engine+0x18,scene);p.put_uint(engine+0x1c,camera)
    def seed_block(mu,address,size,_):
        if address in (0x5da6ca,0x5f9f10):p.set_reg('EBX',0)
    p.mu.hook_add(p.uc.UC_HOOK_CODE,seed_block)
    # Execute only actual game caller block; whole surrounding game object and
    # preceding loader/event transaction are intentionally not constructed.
    p.run(0x5da6ca,stop_at=0x5da727)
    check(p.reg('EIP')==0x5da727 and p.uint(manager+0x14)==camera,
          'actual game block chooses engine DefaultCamera as sky attachment root')
    check(list_entries(p,manager)==skies and all(p.uint(sky+0x2c)==camera for sky in skies),
          'same-scene snapshot reparent does not endlessly chase its mutated source list')
    check(p.uint(camera+0xb0)&0x100,'actual game caller activates default camera hierarchy100')
    p.put_floats(camera+0x20,(10.,20.,30.));p.put_uint(camera+0xb0,p.uint(camera+0xb0)|1)
    p.put_floats(camera+0x40,(0.,1.,0.,-1.,0.,0.,0.,0.,1.))
    f.call(0x45a7d0,this=p.uint(0x75db90))
    identity=(1.,0.,0.,0.,1.,0.,0.,0.,1.)
    check(all(p.floats(sky+0x74,3)==(10.,20.,30.) and p.floats(sky+0x8c,9)==identity
              for sky in skies),'actual camera-follow from parent position, orientation reset by SkyBox override')
    p.mu.mem_write(manager+0x10,b'\0')
    p.run(0x5f9f10,stop_at=0x5f9f6a)
    check(p.reg('EIP')==0x5f9f6a and p.uint(manager+0x14)==camera and
          bytes(p.mu.mem_read(manager+0x10,1))==b'\1',
          'second actual game block sets same camera root and enables sky pass')
    # Engine storage was only borrowed fixture; source scene owns camera/tree.
    p.put_uint(engine+0x18,0);p.put_uint(engine+0x1c,0)
    f.close()

def main():
    lifetime_and_initialization()
    snapshot_attach_and_detach()
    world_and_dispatch()
    dedicated_sky_draw()
    game_default_camera_binding()
    print(f'PASS {checks}/{checks}: PC SkyBox manager lifetime/world/dispatch')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
