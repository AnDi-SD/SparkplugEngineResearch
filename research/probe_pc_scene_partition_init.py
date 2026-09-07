#!/usr/bin/env python3
"""Original PC SceneInit/Partition attachment with explicit Visibility storage.

Visibility whole ctor46D1C0 exceeds the fixed cap at reserve46C0F0. This probe
does NOT replace that function with success or claim the constructor ran:
it provides a documented borrowed manager record and separately executes real
scratch constructors/pool operations. Native SceneInit, graph factories,
attachment/reset/destruction are original. D3D query is a recording boundary.
Imported CRT memmove is a bounded memory-only fixture, no DLL/OS loading.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_static_render_runtime import StaticFixture

checks = 0


def check(value,label):
    global checks
    checks += 1
    if not value: raise AssertionError(label)


class VisibilityFixture(StaticFixture):
    RX_VIS = 0x34110000

    def __init__(self):
        super().__init__()
        p = self.p
        for record,identity,parent in ((0x75e1b8,0x67672341,0x755310),
                                        (0x761458,0x61254ab3,0x75dd88),
                                        (0x762730,0x912cc341,0x75dd88),
                                        (0x7605e0,0x3d7f4387,0x755310)):
            p.put_uint(record,identity)
            p.put_uint(record+0x48,parent)
        self.visibility = p.allocate(0xac)  # borrowed explicit prefix, not factory
        p.mu.mem_write(self.visibility, b'\xcc'*0xac)
        self.call(0x40e910,this=self.visibility) # only actual Base constructor
        p.put_uint(self.visibility,0x6e8cdc)
        p.put_uint(self.visibility+0x10,0x6e8cd8)
        for offset in (0x14,0x1c,0x20,0x24,0x2c,0x30,0x34,0x38,0x44,0x48,0x4c,0x50):
            p.put_uint(self.visibility+offset,0)
        p.mu.mem_write(self.visibility+0x3c,b'\1')
        # Original46C0F0(manager+40,32) preparation is explicitly omitted;
        # capacity-only vs logical-size contract is not yet proved.
        # SceneSetPartition immediately clears this vector in either case.
        # C18 correction:491660 changes logical vertex count, not capacity-only
        # reserve. These real ctor calls initialize two32-node circular rings.
        for offset in (0x54,0x80):
            self.call(0x4914e0,this=self.visibility+offset)
            self.call(0x491660,this=self.visibility+offset,args=(32,))
        p.put_uint(0x75e1b0,self.visibility)
        p.mu.mem_map(self.RX_VIS,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
        device,table = p.allocate(4),p.allocate(0x1dc)
        p.put_uint(device,table)
        p.put_uint(table+0x1d8,self.RX_VIS+0x10)
        p.put_uint(self.renderer+0xc9e8,device)
        self.query_calls = 0

        def query(p):
            check(tuple(p.uint(p.reg('ESP')+4*i) for i in (1,2,3))==(device,9,0),
                  'SceneInit original flare query passes device/type9/null output')
            self.query_calls += 1
            p.fixture_return(12,eax=0x8876086a)
        p.seams[self.RX_VIS+0x10] = query
        check(bytes(p.mu.mem_read(0x13bca6c,2))==b'\xff\x25',
              'output erase reaches original imported memmove indirect jump')
        p.put_uint(p.uint(0x13bca6e),self.RX_VIS+0x20)
        self.memmove_calls = []

        def memmove(p):
            dest,source,count = (p.uint(p.reg('ESP')+4*i) for i in (1,2,3))
            check(count<=4096,'bounded memory-only CRT memmove length')
            self.memmove_calls.append((dest,source,count))
            if count: p.mu.mem_write(dest,bytes(p.mu.mem_read(source,count)))
            p.fixture_return(eax=dest) # cdecl, caller retains/pops arguments
        p.seams[self.RX_VIS+0x20] = memmove

    def initialized_scene(self):
        scene = self.scene()
        # Undo LightSceneFixture's older isolated assignment. The original
        # whole Init below must now establish the dependency itself.
        self.p.put_uint(self.p.uint(scene+0x34)+0x1c,0)
        check(self.call(0x45d850,this=scene)&255 == 1,
              'original whole SceneInit succeeds with explicit initialized dependencies')
        return scene

    def close(self):
        # Scene teardown may still reset Visibility, so it must precede fixture
        # teardown. Borrowed manager has no allocation/dtor-factory claim.
        for scene in list(self.scenes): self.delete_scene(scene)
        for offset in (0x54,0x80): self.call(0x491510,this=self.visibility+offset)
        self.call(0x45f460,this=self.visibility)
        self.call(0x4102b0,this=self.visibility)
        self.p.put_uint(0x75e1b0,0)
        self.call(0x6d7fa0) # original global polygon-node pool shutdown
        super().close()


def scene_initialization():
    f = VisibilityFixture()
    p = f.p
    scene = f.initialized_scene()
    system = p.uint(scene+0x3c)
    root = p.uint(system+0x1d4)
    zone = p.uint(root+0x60)
    check(p.uint(scene+0x38)==system and f.allocations[system]==0x1d8 and
          f.allocations[root]==0x84 and f.allocations[zone]==0xc8,
          'actual SceneInit creates exact fallback System/Root/Zone and selects fallback')
    check(p.uint(system+0x2c)==p.uint(scene+0x14) and p.uint(zone+0x2c)==system,
          'actual Node attachment gives SystemRoot->PartitionSystem->Zone tree')
    check(p.uint(root+0x74)==system and p.uint(root+0x80)==scene and
          p.uint(system+0x3c)==p.uint(zone+0x3c)==scene,
          'actual typed scene propagation reaches partition root and Zone')
    check(p.uint(zone+0xbc)-p.uint(zone+0xb8)==4 and p.uint(p.uint(zone+0xb8))==root,
          'actual Zone local-root vector points at fallback root')
    check(p.uint(system+8)&65535==3 and p.uint(root+8)&65535==0 and
          p.uint(zone+8)&65535==2,
          'Scene selected/fallback plus Node refs; direct root; Zone child+partition refs')
    check(p.uint(scene+0x20)==0,
          'PartitionSystem RTTI skips RenderNode: not incorrectly inserted in ordinary render list')
    check(p.uint(p.uint(scene+0x34)+0x1c)==scene+0x18 and f.query_calls==1,
          'actual LightManager gets render-list address and flare query occurs once')
    f.close()
    check(system in f.freed and root in f.freed and zone in f.freed,
          'whole Scene destructor tears down native fallback graph')


def dynamic_registration_and_partition_transfer():
    f = VisibilityFixture()
    p = f.p
    scene = f.initialized_scene()
    old = p.uint(scene+0x38)
    old_root = p.uint(old+0x1d4)
    node = f.node()
    f.call(0x421a60,this=p.uint(scene+0x14),args=(node,))
    f.nodes.remove(node)  # actual scene Node tree now owns the RenderNode
    f.call(0x45a7d0,this=p.uint(0x75db90))
    check(p.uint(old_root+0x28)-p.uint(old_root+0x24)==4 and
          p.uint(p.uint(old_root+0x24))==node and
          p.uint(p.uint(node+0x1c8))==old_root,
          'actual scene world update registers RenderNode in fallback partition')
    new = f.call(0x48e7c0)
    root = f.call(0x426910)
    zone = f.call(0x480fd0)
    f.call(0x421a60,this=new,args=(zone,))
    p.put_uint(new+0x1d4,root)
    p.put_uint(root+0x74,new)
    p.put_uint(root+0x60,zone)
    p.mu.mem_write(zone+8,(2).to_bytes(2,'little')) # Node owner plus literal root ref
    f.call(0x481080,this=zone,args=(root,))
    f.call(0x421a60,this=p.uint(scene+0x14),args=(new,))
    check(p.uint(scene+0x38)==new and p.uint(old+0x2c)==0,
          'typed Partition attachment selects new system and detaches previous fallback')
    check(p.uint(root+0x28)-p.uint(root+0x24)==4 and
          p.uint(p.uint(root+0x24))==node and p.uint(p.uint(node+0x1c8))==root,
          'actual48E940 transfers reciprocal RenderNode membership to new root')
    check(p.uint(old_root+0x24)==p.uint(old_root+0x28)==p.uint(old_root+0x2c)==0 and
          p.uint(root+0x80)==scene,
          'old root reset follows transfer; new root receives Scene')
    check(p.uint(scene+0x20)==1,
          'partition switch preserves single ordinary RenderNode list entry')
    f.close()
    check(all(obj in f.freed for obj in (node,new,root,zone,old,old_root)),
          'whole scene teardown releases selected and detached fallback ownership')


def main():
    scene_initialization()
    dynamic_registration_and_partition_transfer()
    print(f'PASS {checks}/{checks}: original SceneInit/partition with explicit Visibility dependency')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
