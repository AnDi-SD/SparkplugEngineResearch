#!/usr/bin/env python3
"""Original PC scene creation/registration/root world dispatch in bounded guest.

Scene and its four sub-manager factories/destructors are original. String-owner
allocation is an explicit existing fixture, as are absent engine storage and
clone-pair recording. No scene rendering/initialization or Windows/GPU API runs.
"""
from pathlib import Path
import sys

from pc_instruction_emulator import run_bounded
from probe_pc_san_reader import ReaderFixture, cstring

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def scene_entries(p, manager):
    sentinel = p.uint(manager + 0x18)
    link = p.uint(sentinel)
    result = []
    previous = sentinel
    while link != sentinel:
        check(len(result) < 16 and p.uint(link + 4) == previous, 'bounded bidirectional scene list')
        result.append(p.uint(link + 8))
        previous, link = link, p.uint(link)
    check(p.uint(sentinel + 4) == previous and len(result) == p.uint(manager + 0x1c),
          'scene sentinel tail/count agree')
    return result


class SceneFixture(ReaderFixture):
    def __init__(self):
        super().__init__(b'')
        self.call(0x6d38e0)
        self.p.put_uint(0x755274, self.p.allocate(0x160))
        self.scenes = []

    def scene(self):
        result = self.call(0x45ebc0)
        self.scenes.append(result)
        return result

    def delete_scene(self, scene):
        self.call(0x45eb50, this=scene, args=(1,))
        self.scenes.remove(scene)

    def close(self):
        p = self.p
        for scene in list(self.scenes):
            self.delete_scene(scene)
        manager = p.uint(0x75db90)
        if manager:
            check(scene_entries(p, manager) == [], 'scenes unregister before manager lifetime ends')
            self.call(0x45add0, this=manager, args=(1,))
        for address in (0x75db98, 0x75526c):
            obj = p.uint(address)
            if obj and obj not in self.freed:
                self.call(p.uint(p.uint(obj)), this=obj, args=(1,))
        check(set(self.freed) == set(self.allocations), 'all tracked original scene allocations released')


def lifecycle():
    f = SceneFixture()
    p = f.p
    manager = f.call(0x45adf0)
    check(f.allocations[manager] == 0x24 and p.uint(manager) == 0x6e7154,
          'manager exact24 factory/vtable')
    check(p.uint(manager + 0x10) == 0x6e7150 and p.uint(manager + 0x14) == 0xcccccccc,
          'singleton support and untouched list allocator')
    check(p.uint(0x75db90) == manager and p.uint(manager + 0x20) == 0,
          'manager singleton published/current scene null')
    check(scene_entries(p, manager) == [], 'manager starts with owned empty sentinel')
    p.put_uint(manager + 0x20, 0x12345678)
    f.call(0x45a7d0, this=manager)
    check(p.uint(manager + 0x20) == 0, 'empty update also resets current scene')
    scenes = [f.scene() for _ in range(3)]
    check(scene_entries(p, manager) == scenes, 'native scene constructor appends in creation order')
    for scene in scenes:
        root = p.uint(scene + 0x14)
        check(f.allocations[scene] == 0x54 and p.uint(scene) == 0x6e7358,
              'scene exact54 allocation/vtable')
        check(p.uint(scene + 0x10) == 0 and p.uint(scene + 0x44) == 0xcccccccc,
              'scene initially unnamed; vector allocator44 untouched')
        check(bytes(p.mu.mem_read(scene + 0x24, 4)) == b'\0\1\xcc\xcc',
              'independent scene flags24/25 and untouched padding')
        check(all(p.uint(scene + offset) == 0 for offset in (0x18, 0x1c, 0x20, 0x38, 0x3c, 0x40, 0x48, 0x4c, 0x50)),
              'render list/partition/sorted buffer initially empty')
        check(f.allocations[root] == 0xb4 and p.uint(root) == 0x6dc4f4 and
              p.uint(root + 8) & 65535 == 1, 'scene retains actual original spNode root once')
        check(cstring(p, p.uint(root + 0x10) + 9) == b'System Root' and
              p.uint(root + 0x3c) == scene and p.uint(root + 0xb0) == 0x70b00,
              'original root name/scene backpointer/bit100')
        for offset, size in ((0x28, 0x38), (0x2c, 0x24), (0x30, 0x24), (0x34, 0x24)):
            check(f.allocations[p.uint(scene + offset)] == size,
                  'original scene-owned manager factory allocation')
        check(p.uint(p.uint(scene + 0x34) + 0x20) == scene,
              'light manager receives borrowed owner scene')
    removed_root = p.uint(scenes[1] + 0x14)
    f.delete_scene(scenes[1])
    check(scene_entries(p, manager) == [scenes[0], scenes[2]] and removed_root in f.freed,
          'middle scene unregisters and releases root through native teardown')
    f.close()


def world_dispatch():
    f = SceneFixture()
    p = f.p
    scenes = [f.scene() for _ in range(3)]
    manager = p.uint(0x75db90)
    roots = [p.uint(scene + 0x14) for scene in scenes]
    observed = []

    def observe(mu, address, size, _):
        if address == 0x421420 and p.reg('ECX') in roots:
            root = p.reg('ECX')
            check(p.uint(p.reg('ESP') + 4) == 0, 'scene manager passes inheritedFlags zero')
            observed.append((p.uint(manager + 0x20), root))

    p.mu.hook_add(p.uc.UC_HOOK_CODE, observe)
    for i, root in enumerate(roots):
        p.put_floats(root + 0x20, (10. + i, 20. + i, 30. + i))
        p.put_uint(root + 0xb0, (p.uint(root + 0xb0) | 1) & ~0x200)
        p.mu.mem_write(scenes[i] + 0x24, bytes((i % 2, i % 2)))
    f.call(0x45a7d0, this=manager)
    check(observed == list(zip(scenes, roots)), 'world dispatch follows scene order without scene/Enabled gates')
    check(p.uint(manager + 0x20) == 0, 'current scene clears after final callback')
    for i, root in enumerate(roots):
        check(p.floats(root + 0x74, 3) == (10. + i, 20. + i, 30. + i),
              'actual original world matrix path copies dirty root local position')
    f.close()


def manager_borrowed_scene_and_clone():
    f = SceneFixture()
    p = f.p
    manager = f.call(0x45adf0)
    sentinel = p.uint(manager + 0x18)
    # Literal list storage isolates manager ownership: scene bytes are borrowed,
    # not a native constructed scene that would consult a replaced singleton.
    fake_scene = p.allocate(0x54)
    link = p.allocate(12)
    f.allocations[link] = 12
    p.put_uint(link, sentinel)
    p.put_uint(link + 4, sentinel)
    p.put_uint(link + 8, fake_scene)
    p.put_uint(sentinel, link)
    p.put_uint(sentinel + 4, link)
    p.put_uint(manager + 0x1c, 1)
    p.put_uint(manager + 0x20, fake_scene)
    clone_manager = p.allocate(0x24)
    p.put_uint(0x74e060, clone_manager)
    pairs = []

    def pair(p):
        check(p.reg('ECX') == clone_manager, 'clone registration receiver')
        pairs.append((p.uint(p.reg('ESP') + 4), p.uint(p.reg('ESP') + 8)))
        p.fixture_return(8)

    p.seams[0x412f70] = pair
    cloned = f.call(0x45ae50, this=manager)
    check(pairs == [(manager, cloned)] and p.uint(0x75db90) == cloned,
          'clone factory publishes new singleton and records pair')
    check(scene_entries(p, cloned) == [] and p.uint(cloned + 0x20) == 0,
          'clone is fresh: list and current scene not copied')
    f.call(0x45add0, this=manager, args=(1,))
    check(p.uint(0x75db90) == 0 and link in f.freed and fake_scene not in f.freed,
          'old manager dtor clears singleton unconditionally; only list nodes owned')
    f.call(0x45add0, this=cloned, args=(1,))
    f.close()


def native_child_edges():
    f = SceneFixture()
    p = f.p
    first, second = f.scene(), f.scene()
    roots = [p.uint(scene + 0x14) for scene in (first, second)]
    parent = f.call(0x421e20)
    child = f.call(0x421e20)
    f.call(0x421a60, this=parent, args=(child,))
    check(p.uint(child + 0x2c) == parent and p.uint(child + 0x3c) == 0,
          'native unattached hierarchy can precede scene membership')
    check(p.uint(child + 8) & 65535 == 1, 'native child list owns one intrusive reference')
    f.call(0x421a60, this=roots[0], args=(parent,))
    check(p.uint(parent + 0x3c) == p.uint(child + 0x3c) == first,
          'native Attach routes entire subtree into parent scene')
    check(all(p.uint(node + 0xb0) & 0x100 for node in (parent, child)),
          'root bit100 propagates recursively to inserted descendants')
    before = len(f.requests)
    f.call(0x421a60, this=roots[0], args=(parent,))
    check(p.uint(roots[0] + 0x1c) == 1 and len(f.requests) == before,
          'reattach to same parent is an early no-op')
    f.call(0x421a60, this=roots[1], args=(parent,))
    check(p.uint(roots[0] + 0x1c) == 0 and p.uint(roots[1] + 0x1c) == 1 and
          p.uint(parent + 0x2c) == roots[1], 'reparent transfers owned list edge')
    check(p.uint(parent + 0x3c) == p.uint(child + 0x3c) == second,
          'reparent recursively detaches old scene and attaches new scene')
    p.put_floats(roots[1] + 0x20, (10., 20., 30.))
    p.put_uint(roots[1] + 0xb0, p.uint(roots[1] + 0xb0) | 1)
    p.put_floats(parent + 0x20, (1., 2., 3.))
    p.put_floats(child + 0x20, (4., 5., 6.))
    f.call(0x45a7d0, this=p.uint(0x75db90))
    check(p.floats(parent + 0x74, 3) == (11., 22., 33.) and
          p.floats(child + 0x74, 3) == (15., 27., 39.),
          'original scene-manager frame traverses original owned node hierarchy')
    f.close()
    check(parent in f.freed and child in f.freed, 'scene teardown releases native subtree ownership')


def main():
    lifecycle()
    world_dispatch()
    manager_borrowed_scene_and_clone()
    native_child_edges()
    print(f'PASS {checks}/{checks}: original PC scene lifecycle/list/world dispatch')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
