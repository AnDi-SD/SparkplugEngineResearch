#!/usr/bin/env python3
"""Bounded original PC render-node lifetime and intrusive scene registration.

The PE factory, constructors, Attach/reparent, typed registration, list edits,
and destructors run unchanged. Explicit preinitialized RTTI records and zero
renderer storage replace unavailable startup/device state, not those methods.
"""
from pathlib import Path
import sys

from pc_instruction_emulator import run_bounded
from probe_pc_scene_world import SceneFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class RenderSceneFixture(SceneFixture):
    def __init__(self):
        super().__init__()
        p = self.p
        self.renderer = 0x35000000
        p.mu.mem_map(self.renderer, 0x10000, p.uc.UC_PROT_READ | p.uc.UC_PROT_WRITE)
        p.put_uint(0x75db68, self.renderer)
        # Native IsKindOf follows record+48. This explicit startup state is
        # limited to the actual hierarchy used here; no whole-CRT claim.
        for record, identity, parent in (
                (0x755310, 0x415352a1, 0),
                (0x7555f8, 0x44de07fd, 0x755310),
                (0x75dd88, 0x695c0f65, 0x7555f8),
                (0x75e150, 0x603625d0, 0x75dd88)):
            p.put_uint(record, identity)
            p.put_uint(record + 0x48, parent)

    def render_node(self):
        return self.call(0x425520)


def render_entries(p, scene):
    result = []
    previous, current = 0, p.uint(scene + 0x18)
    while current:
        check(len(result) < 16 and p.uint(current + 0x128) == previous,
              'intrusive render prev agrees and traversal stays bounded')
        result.append(current)
        previous, current = current, p.uint(current + 0x12c)
    check(p.uint(scene + 0x1c) == previous and p.uint(scene + 0x20) == len(result),
          'intrusive scene render tail/count agree')
    return result


def constructor_and_cache_teardown():
    f = RenderSceneFixture()
    p = f.p
    node = f.render_node()
    check(f.allocations[node] == 0x1d4 and p.uint(node) == 0x6dcaa4,
          'original factory exact1D4 and primary vtable')
    check(p.uint(node + 0xb4) == 0x6dcadc and p.uint(node + 0x124) == node,
          'secondary support vptr and complete-object self link')
    check(all(p.uint(node + off) == 0 for off in (
        0xbc, 0xc0, 0xc4, *range(0xc8, 0xe8, 4), *range(0xf0, 0x120, 4),
        0x128, 0x12c, 0x134, 0x1c8, 0x1cc, 0x1d0)),
        'empty vectors, spheres, cache, scene links, dirty flags')
    check(p.uint(node + 0xb8) == p.uint(node + 0x1c4) == 0xcccccccc and
          p.uint(node + 0x130) == 0xcccccc00,
          'allocator/padding bytes are not invented zero defaults')
    check(p.uint(node + 0x120) == 0x01010100,
          'support byte flags exactly 00 01 01 01')
    identity = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)
    check(p.floats(node + 0x138, 16) == p.floats(node + 0x178, 16) == identity and
          p.floats(node + 0x1b8, 3) == (1., 1., 1.),
          'two identity matrices and inverse scale defaults')
    check(p.uint(node + 0xe8) == node + 0x138 and p.uint(node + 0xec) == node + 0x178,
          'support matrix pointers alias node-owned caches')
    p.put_uint(f.renderer + 0xc190, node + 0xf0)
    f.call(0x4255d0, this=node, args=(1,))
    check(p.uint(f.renderer + 0xc190) == 0, 'support dtor clears matching renderer cache')
    for matches, busy in ((True, 1), (False, 0)):
        node = f.render_node()
        value = node + 0xf0 if matches else 0x12345678
        p.put_uint(f.renderer + 0xc190, value)
        p.put_uint(f.renderer + 0xc9c4, busy)
        f.call(0x4255d0, this=node, args=(1,))
        check(p.uint(f.renderer + 0xc190) == value,
              'support dtor preserves busy or unrelated renderer cache')
    p.put_uint(f.renderer + 0xc190, 0)
    p.put_uint(f.renderer + 0xc9c4, 0)
    f.close()


def native_registration_and_reparent():
    f = RenderSceneFixture()
    p = f.p
    first, second = f.scene(), f.scene()
    roots = [p.uint(scene + 0x14) for scene in (first, second)]
    nodes = [f.render_node() for _ in range(3)]
    check(render_entries(p, first) == render_entries(p, second) == [], 'empty scene lists')
    for node in nodes:
        f.call(0x421a60, this=roots[0], args=(node,))
        check(p.uint(node + 0x3c) == first and p.uint(node + 8) & 65535 == 1,
              'typed registration adds borrowed scene link, not a second reference')
    check(render_entries(p, first) == nodes, 'native typed registration appends creation order')
    count = len(f.requests)
    f.call(0x421a60, this=roots[0], args=(nodes[1],))
    check(render_entries(p, first) == nodes and len(f.requests) == count,
          'same-parent no-op does not duplicate intrusive registration')
    for i, expected in ((1, [nodes[0], nodes[2]]), (0, [nodes[2]]), (2, [])):
        f.call(0x421a60, this=roots[1], args=(nodes[i],))
        check(render_entries(p, first) == expected, 'middle/head/tail native unlink')
        check(p.uint(nodes[i] + 0x3c) == second and p.uint(nodes[i] + 8) & 65535 == 1,
              'reparent updates scene without losing child ownership')
    check(render_entries(p, second) == [nodes[1], nodes[0], nodes[2]],
          'destination append order follows native reparent order')
    # A subtree is registered recursively, not only its directly attached node.
    branch = f.call(0x421e20)
    extra = f.render_node()
    f.call(0x421a60, this=branch, args=(extra,))
    check(p.uint(extra + 0x3c) == 0, 'unattached render subtree has no scene')
    f.call(0x421a60, this=roots[0], args=(branch,))
    check(render_entries(p, first) == [extra] and p.uint(extra + 0x3c) == first,
          'native traversal reaches render descendant below ordinary node')
    f.call(0x421a60, this=roots[1], args=(branch,))
    check(render_entries(p, first) == [] and render_entries(p, second) ==
          [nodes[1], nodes[0], nodes[2], extra], 'recursive typed reparent updates both lists')
    f.delete_scene(second)
    check(all(node in f.freed for node in (*nodes, branch, extra)),
          'scene teardown unregisters render links before owned subtree destruction')
    f.close()


def main():
    constructor_and_cache_teardown()
    native_registration_and_reparent()
    print(f'PASS {checks}/{checks}: original PC render-node constructor/scene registry/teardown')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
