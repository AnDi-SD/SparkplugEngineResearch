#!/usr/bin/env python3
"""Bounded PC PartitionNode/Zone/PartitionSystem runtime; no renderer/OS calls.

Original factories/clone map/lifetime execute. Explicit registered parent
records distinguish the native RTTI graph from the physical C++ render prefix.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_render_node_ownership import OwnedFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value: raise AssertionError(label)


class PartitionFixture(OwnedFixture):
    def __init__(self):
        super().__init__()
        self.spatial = []
        for record, identity, parent in ((0x75e1b8, 0x67672341, 0x755310),
                                         (0x761458, 0x61254ab3, 0x75dd88),
                                         (0x762730, 0x912cc341, 0x75dd88)):
            self.p.put_uint(record, identity)
            self.p.put_uint(record + 0x48, parent)

    def spatial_object(self, factory):
        obj = self.call(factory)
        self.spatial.append(obj)
        return obj

    def close(self):
        for obj in reversed(self.spatial):
            self.call(self.p.uint(self.p.uint(obj)), this=obj, args=(1,))
        super().close()


def vector(p, obj, offset):
    begin, end = p.uint(obj + offset + 4), p.uint(obj + offset + 8)
    check(0 <= end - begin <= 128 and (end - begin) % 4 == 0,
          'bounded native pointer vector')
    return [p.uint(i) for i in range(begin, end, 4)]


def lifetime_and_rtti():
    f = PartitionFixture()
    p = f.p
    root = f.spatial_object(0x426910)
    zone = f.spatial_object(0x480fd0)
    system = f.spatial_object(0x48e7c0)
    check(f.allocations[root] == 0x84 and p.uint(root) == 0x6dcb08,
          'actual PartitionNode exact84, not guessed80')
    check(f.allocations[zone] == 0xc8 and p.uint(zone) == 0x6ebacc,
          'actual Zone exactC8 Node-derived physical layout')
    check(f.allocations[system] == 0x1d8 and p.uint(system) == 0x6ec528 and
          p.uint(system + 0xb4) == 0x6ec510,
          'actual PartitionSystem exact1D8 contains RenderNode support prefix')
    check(f.call(0x408370, this=root, args=(0x415352a1,)) & 255 == 1 and
          f.call(0x408370, this=root, args=(0x695c0f65,)) & 255 == 0,
          'spPartitionNode is BaseObject, not scene spNode')
    check(f.call(0x408370, this=system, args=(0x695c0f65,)) & 255 == 1 and
          f.call(0x408370, this=system, args=(0x603625d0,)) & 255 == 0,
          'actual RTTI PartitionSystem skips RenderNode despite physical RenderNode inheritance')
    check(p.uint(root + 0x50) == 0xffffffff and all(p.uint(root + i) == 0 for i in
          (0x14,0x18,0x1c,0x24,0x28,0x2c,0x34,0x38,0x3c,0x44,0x48,0x4c,
           0x54,0x58,0x5c,0x60,0x68,0x6c,0x70,0x74,0x78,0x7c,0x80)),
          'partition empty vectors, child storage, owner links and debugColorFFFFFFFF')
    check(all(p.uint(root + i) == 0xcccccccc for i in (0x10,0x20,0x30,0x40,0x64)),
          'five compiler vector allocator words untouched')
    check(p.uint(zone + 0xb4) == p.uint(zone + 0xc4) == 0xcccccccc and
          all(p.uint(zone + i) == 0 for i in (0xb8,0xbc,0xc0)),
          'zone root vector empty; trailing C4 untouched, not invented null field')
    check(p.uint(system + 0x1d4) == 0 and p.uint(system + 0xb0) == 0x70e00,
          'system root initially null and additional Node flag400 set')
    p.put_uint(root + 0x50, 0x11223344)
    p.put_uint(zone + 0xc4, 0x12345678)
    for obj, expected_size, check_offset, expected in (
            (root,0x84,0x50,0xffffffff), (zone,0xc8,0xc4,0xcccccccc),
            (system,0x1d8,0x1d4,0)):
        # Actual native object clone, with the already initialized static map.
        clone = f.call(p.uint(p.uint(obj) + 8), this=obj)
        f.spatial.append(clone)
        check(f.allocations[clone] == expected_size and p.uint(clone + check_offset) == expected,
              'native inherited-only clone leaves own partition/zone fields at defaults')
    f.close()


def borrowed_roots_and_render_nodes():
    f = PartitionFixture()
    p = f.p
    root = f.spatial_object(0x426910)
    zone = f.spatial_object(0x480fd0)
    for _ in range(2): f.call(0x481080, this=zone, args=(root,))
    check(vector(p, zone, 0xb4) == [root, root] and p.uint(root + 8) & 65535 == 0,
          'Zone roots append allows duplicates and takes no reference')
    f.call(0x480fb0, this=zone, args=(1,))
    f.spatial.remove(zone)
    check(root not in f.freed, 'Zone dtor frees root storage, not borrowed roots')
    a, b, c = f.node(), f.node(), f.node()
    for node in (a, b, c, a): f.call(0x426690, this=root, args=(node,))
    check(vector(p, root, 0x20) == [a, b, c, a] and
          vector(p, a, 0x1c4) == [root, root] and
          all(p.uint(obj+8) & 65535 == 0 for obj in (a,b,c,root)),
          'native registration appends reciprocal borrowed links including duplicates')
    f.call(0x425b60, this=root, args=(b, 1))
    check(vector(p, root, 0x20) == [a, a, c] and vector(p,b,0x1c4) == [],
          'Partition remove-one swaps last then actual RenderNode callback unlinks reverse')
    f.call(0x424d60, this=a, args=(root, 1))
    check(vector(p, root, 0x20) == [c, a] and vector(p,a,0x1c4) == [root],
          'RenderNode remove-one reciprocally unlinks one Partition occurrence')
    f.call(0x425bc0, this=root, args=(1,))
    check(vector(p,root,0x20) == [] and vector(p,a,0x1c4) == [] and
          vector(p,c,0x1c4) == [], 'native notifying partition drain leaves both sides empty')
    for node in (a,b,c): f.call(0x426690,this=root,args=(node,))
    f.call(0x4255d0,this=b,args=(1,))
    f.nodes.remove(b)
    check(vector(p,root,0x20) == [a,c],
          'actual RenderNode destructor unregisters itself from partition')
    f.call(0x426890,this=root,args=(1,))
    f.spatial.remove(root)
    check(vector(p,a,0x1c4) == vector(p,c,0x1c4) == [],
          'actual Partition destructor unregisters itself from remaining render nodes')
    f.close()


def scene_propagation_and_owning_graph():
    f = PartitionFixture()
    p = f.p
    root = f.spatial_object(0x426910)
    child = f.spatial_object(0x426910)
    zone = f.spatial_object(0x480fd0)
    system = f.spatial_object(0x48e7c0)
    # Literal assignments reproduce the separately decoded SceneInit graph.
    # Child-array installation is a fixture, not a claimed original setter.
    children = p.allocate(8)
    f.allocations[children] = 8
    p.put_uint(children, child)
    p.put_uint(root+0x58, children)
    p.put_uint(root+0x5c, 2)
    p.put_uint(child+0x54, root)
    p.put_uint(root+0x74, system)
    p.put_uint(system+0x1d4, root)
    p.put_uint(root+0x60, zone)
    p.mu.mem_write(zone+8, (1).to_bytes(2,'little'))
    f.call(0x481080,this=zone,args=(root,))
    p.mu.mem_write(root+8, (3).to_bytes(2,'little'))
    p.mu.mem_write(child+8, (2).to_bytes(2,'little'))
    scene = p.allocate(0x54)  # borrowed identity only; no Scene traversal
    f.call(0x4259e0,this=root,args=(scene,))
    check(p.uint(root+0x80) == p.uint(child+0x80) == scene,
          'Scene propagation traverses nonnull children and skips null child slots')
    f.call(0x48e870,this=system,args=(1,))
    for obj in (root,child,zone,system): f.spatial.remove(obj)
    check(all(obj in f.freed for obj in (root,child,zone,system,children)),
          'system owns root directly; root owns child directly despite nonzero refcounts; Zone intrusive')
    f.close()


def nonnotifying_vector_reset():
    f = PartitionFixture()
    p = f.p
    root = f.spatial_object(0x426910)
    node = f.node()
    f.call(0x426690,this=root,args=(node,))
    storage = p.uint(root+0x24)
    f.call(0x4269c0,this=root)
    check(vector(p,root,0x20) == [] and storage in f.freed and
          vector(p,node,0x1c4) == [root],
          'root v80 frees own dynamic vector WITHOUT touching reciprocal registrations')
    # Actual call site48E940 transfers/unregisters objects before48E790 invokes
    # this reset. Restore the deliberately broken graph through native clear,
    # otherwise a later node destructor would notify a destroyed partition.
    f.call(0x424dd0,this=node,args=(0,))
    f.close()


def main():
    lifetime_and_rtti()
    borrowed_roots_and_render_nodes()
    scene_propagation_and_owning_graph()
    nonnotifying_vector_reset()
    print(f'PASS {checks}/{checks}: original PC spatial class lifetime/RTTI')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
