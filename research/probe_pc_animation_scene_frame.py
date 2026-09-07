#!/usr/bin/env python3
"""Original PC app->timer->SAN actor->scene->world chain, no forced world update.

Real default-scene/DXCamera creation, native timer attachment and node parent
edges. Full scene initializer, unrelated update managers, event dispatcher,
foreground API and graphics stay explicit seams. No game/GPU launch.
"""
from pathlib import Path
import math
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_engine_frame import FrameFixture, UPDATE_ORDER
from probe_pc_scene_world import scene_entries

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def main():
    f = FrameFixture()
    p = f.p
    # Replace the scene manager placeholder by its original singleton/factory.
    del p.seams[0x45a7d0]
    p.put_uint(0x75db90, 0)
    scene_init = []

    def initialize(p):
        scene_init.append(p.reg('ECX'))
        p.fixture_return(eax=1)

    p.seams[0x45d850] = initialize
    root_timer, child_timer = f.engine + 0x54, f.engine + 0x90
    f.call(0x4506f0, this=root_timer, args=(0,))
    f.call(0x4506f0, this=child_timer, args=(0,))
    check(f.call(0x41c300, this=f.engine) & 255 == 1, 'actual default scene/camera/timer setup')
    scene = p.uint(f.engine + 0x18)
    camera = p.uint(f.engine + 0x1c)
    scene_manager = p.uint(0x75db90)
    root = p.uint(scene + 0x14)
    check(scene_init == [scene] and scene_entries(p, scene_manager) == [scene],
          'one original scene, only initialize internals are a seam')
    for node in f.nodes:
        f.call(0x421a60, this=root, args=(node,))
        check(p.uint(node + 0x3c) == scene and p.uint(node + 0x2c) == root,
              'animated nodes use actual scene-owned parent links')
    offset = (10., -20., 30.)
    p.put_floats(root + 0x20, offset)
    p.put_uint(root + 0xb0, p.uint(root + 0xb0) | 1)
    native_world = []

    def observe(mu, address, size, _):
        if address == 0x450750 and p.reg('ECX') == root_timer:
            f.stages.append('clock')
        elif address == 0x45a7d0:
            check(p.reg('ECX') == scene_manager, 'engine calls actual scene singleton')
            f.stages.append('global0075DB90')
        elif address == 0x421420:
            check(p.uint(scene_manager + 0x20) == scene, 'world traversal retains current scene')
            native_world.append(p.reg('ECX'))

    p.mu.hook_add(p.uc.UC_HOOK_CODE, observe)
    p.put_uint(0x74e050, 1)
    p.mu.mem_write(0x75f66c, b'\0')
    p.put_uint(0x75f670, 1000)
    f.call(0x450840, this=root_timer)
    f.start(fade=0)
    before = [p.floats(node + 0x74, 3) for node in f.nodes]
    for milliseconds, expected in ((1250, .25), (1500, .5), (1750, .75)):
        p.put_uint(0x75f670, milliseconds)
        native_world.clear()
        f.stages.clear()
        check(f.call(0x4c2d60, this=f.app) & 255 == 1, 'original app frame succeeds')
        check(f.stages == ['pre-update', *UPDATE_ORDER, 'foreground', 'graphics'],
              'timer/actor/scene integration preserves exact outer frame order')
        check(native_world == [root, *f.nodes], 'scene triggers original preorder world calls exactly once')
        check(p.floats(child_timer + 0x28, 1) == (.25,) and
              p.floats(f.state(0) + 0x34, 1) == (expected,), 'native linked time advances actor sample')
        for node in f.nodes:
            local = p.floats(node + 0x20, 3)
            world = p.floats(node + 0x74, 3)
            check(all(math.isclose(w, l + o, rel_tol=2e-6, abs_tol=2e-6)
                      for w, l, o in zip(world, local, offset)),
                  'same frame applies real SAN keys and inherited scene root translation')
        check(p.uint(scene_manager + 0x20) == 0, 'scene update resets current pointer before graphics')
    check(before != [p.floats(node + 0x74, 3) for node in f.nodes],
          'world caches changed without any explicit test-side UpdateWorld call')
    f.call(0x45eb50, this=scene, args=(1,))
    p.put_uint(f.engine + 0x18, 0)
    check(all(node not in f.freed and p.uint(node + 0x3c) == 0 for node in f.nodes),
          'scene releases parent references; actor keeps animated nodes alive')
    f.call(0x4a91e0, this=camera, args=(1,))
    p.put_uint(f.engine + 0x1c, 0)
    f.call(0x45add0, this=scene_manager, args=(1,))
    camera_manager = p.uint(0x75db98)
    if camera_manager:
        f.call(p.uint(p.uint(camera_manager)), this=camera_manager, args=(1,))
    f.call(0x4506b0, this=child_timer)
    f.call(0x4506b0, this=root_timer)
    f.close()
    print(f'PASS {checks}/{checks}: original app/timers/SAN actor/owned scene/world frame')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
