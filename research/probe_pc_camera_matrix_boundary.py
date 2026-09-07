#!/usr/bin/env python3
"""Original PC camera allocation/matrix offsets and renderer-call arguments.

Only scene fixture ownership/engine storage and renderer leaves are seams.
Neither a renderer/device factory nor a Windows/Direct3D API is invoked.
"""
from pathlib import Path
import math
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_world import SceneFixture

checks = 0
IDENTITY4 = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def main():
    f = SceneFixture()
    p = f.p
    cameras = [f.call(entry) for entry in (0x4a9120, 0x41a3f0)]
    for camera, table in zip(cameras, (0x6ef1e0, 0x6dea20)):
        check(f.allocations[camera] == 0x238 and p.uint(camera) == table,
              'both PC concrete camera factories have exact238 allocation')
        check(p.floats(camera + 0xcc, 16) == IDENTITY4 and
              p.floats(camera + 0x10c, 16) == IDENTITY4,
              'native ctor matrix starts are CC/10C, not old D4/114 documentation')
        check(p.floats(camera + 0x14c, 9) == (0., 0., 1., 0., 1., 0., 1., 0., 0.),
              'default forward/up/right bases start14C')
        check(bytes(p.mu.mem_read(camera + 0x170, 24)) == b'\xcc' * 24,
              'viewport170..187 remains unwritten before configure/render')
        check(p.uint(camera + 0xc8) & 255 == 0 and p.uint(camera + 0x231) & 255 == 0 and
              p.uint(camera + 0x224) == 2, 'ctor angle setter clears2D, leaves branch false and projection dirty')
    camera = cameras[0]
    renderer, table = p.allocate(0x24), p.allocate(0x40)
    p.put_uint(renderer + 0x18, table)
    p.put_uint(0x75db68, renderer)
    p.mu.mem_map(0x34070000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
    calls = []
    view_result, projection_result = 1, 1

    def matrix(kind, expected_offset):
        def invoke(p):
            check(p.reg('ECX') == renderer + 0x18 and
                  p.uint(p.reg('ESP') + 4) == camera + expected_offset,
                  f'original renderer {kind} receives corrected exact matrix address')
            calls.append((kind, p.floats(camera + expected_offset, 16)))
            p.fixture_return(4, eax=view_result if kind == 'view' else projection_result)
        return invoke

    p.put_uint(table + 0x34, 0x34070010)
    p.put_uint(table + 0x30, 0x34070020)
    p.seams[0x34070010] = matrix('view', 0xcc)
    p.seams[0x34070020] = matrix('projection', 0x10c)
    check(f.call(0x427d40, this=camera) & 255 == 1, 'actual camera applies matrices')
    check([name for name, _ in calls] == ['view', 'projection'] and
          p.uint(camera + 0x224) == 0, 'projection cache calculated before renderer, dirty bit cleared')
    check(calls[0][1] == IDENTITY4 and calls[1][1][11] == 1 and calls[1][1][15] == 0,
          'view remains identity; projection perspective constants set')
    perspective = calls[1][1]
    p.mu.mem_write(camera + 0x231, b'\1')
    f.call(0x426f10, this=camera)
    orthographic = p.floats(camera + 0x10c, 16)
    check(orthographic[11] == perspective[11] == 1 and orthographic[15] == perspective[15] == 0,
          'native orthographic branch does NOT reset previous perspective23/33 slots')
    changed = [i for i, (a, b) in enumerate(zip(perspective, orthographic)) if a != b]
    check(set(changed) <= {0, 5, 10, 14}, 'orthographic cache routine writes only four coefficients')
    p.put_floats(camera + 0x10c, IDENTITY4)
    f.call(0x426f10, this=camera)
    check(p.floats(camera + 0x10c, 16)[15] == 1, 'fresh identity yields33=1 only because field is preserved')
    p.mu.mem_write(camera + 0xc8, b'\1')
    angle = p.allocate(4)
    p.put_floats(angle, (0.75,))
    f.call(0x427da0, this=camera, args=(p.uint(angle),))
    check(p.uint(camera + 0xc8) & 255 == 0 and p.uint(camera + 0x231) & 255 == 1,
          'view-angle setter exits2D mode without changing projection branch')
    check(p.floats(camera + 0x188, 1) == (.75,) and p.uint(camera + 0x224) & 2,
          'angle setter value and projection dirty')
    p.put_floats(camera + 0x74, (10., 20., 30.))
    p.put_uint(camera + 0x224, 1)
    calls.clear()
    check(f.call(0x427d40, this=camera) & 255 == 1, 'actual view cache path succeeds')
    expected_view = (*IDENTITY4[:12], -10., -20., -30., 1.)
    check(p.floats(camera + 0xcc, 16) == expected_view and calls[0][1] == expected_view,
          'default basis produces inverse translation at corrected matrix offset')
    view_result = 0
    calls.clear()
    check(f.call(0x427d40, this=camera) & 255 == 0 and [name for name, _ in calls] == ['view'],
          'failed view application suppresses projection call')
    view_result, projection_result = 7, 0
    calls.clear()
    check(f.call(0x427d40, this=camera) & 255 == 0 and len(calls) == 2,
          'nonzero view accepted; projection failure propagated')
    projection_result = 7
    calls.clear()
    check(f.call(0x427d40, this=camera) & 255 == 1, 'projection nonzero result normalized to true')
    for camera in cameras:
        f.call(p.uint(p.uint(camera)), this=camera, args=(1,))
    f.close()
    print(f'PASS {checks}/{checks}: original PC camera allocation/matrix/renderer boundary')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
