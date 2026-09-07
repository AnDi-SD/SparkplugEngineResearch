#!/usr/bin/env python3
"""Original PC render-node world/bounds/matrix/cull/draw boundary, no GPU.

Native node routines and math execute unchanged. Renderable geometry callbacks,
renderer device leaves, and literal camera planes are explicit borrowed fixtures.
The test does not claim to initialize a real renderer or load complete models.
"""
from pathlib import Path
import math
import sys

from pc_instruction_emulator import run_bounded
from probe_pc_scene_render_registry import RenderSceneFixture

checks = 0
IDENTITY = (1., 0., 0., 0., 1., 0., 0., 0., 1.)
IDENTITY4 = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def near(actual, expected):
    return all(math.isclose(a, b, rel_tol=2e-5, abs_tol=2e-5)
               for a, b in zip(actual, expected))


class RuntimeFixture(RenderSceneFixture):
    def __init__(self):
        super().__init__()
        p = self.p
        self.node = self.render_node()
        self.camera = p.allocate(0x238)  # explicit plane storage, not constructed camera
        self.table = p.allocate(0x80)
        p.put_uint(self.renderer + 0x18, self.table)
        p.mu.mem_map(0x34080000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        p.put_uint(self.table + 0x38, 0x34080010)
        self.matrix_result = 1
        self.queued_results = []
        self.calls = []
        self.renderables = []
        p.seams[0x34080010] = self.matrix_call
        p.seams[0x456310] = self.queue_call

    def matrix_call(self, p):
        check(p.reg('ECX') == self.renderer + 0x18 and
              p.uint(p.reg('ESP') + 4) == self.node + 0x138 and
              p.uint(p.reg('ESP') + 8) == self.node + 0x178,
              'original secondary slot14 receives world and inverse matrix pointers')
        self.calls.append(('matrix',))
        p.fixture_return(8, eax=self.matrix_result)

    def queue_call(self, p):
        check(p.reg('ECX') == self.renderer and
              p.uint(p.reg('ESP') + 8) == self.node + 0xb4 and
              p.uint(p.reg('ESP') + 12) == self.camera,
              'queued renderer leaf gets renderable/support/camera argument order')
        self.calls.append(('queue', p.uint(p.reg('ESP') + 4)))
        p.fixture_return(12, eax=self.queued_results.pop(0) if self.queued_results else 1)

    def install_renderables(self, spheres):
        p = self.p
        storage = p.allocate(max(4, len(spheres) * 4))
        self.renderables = []
        for i, sphere in enumerate(spheres):
            obj, table, bounds = p.allocate(0x20), p.allocate(0x34), p.allocate(16)
            p.put_uint(obj, table)
            p.put_floats(bounds, sphere)
            p.put_uint(storage + i * 4, obj)
            self.renderables.append(obj)
            bounds_entry, draw_entry = 0x34080100 + i * 0x20, 0x34080110 + i * 0x20
            p.put_uint(table + 0x2c, bounds_entry)
            p.put_uint(table + 0x24, draw_entry)

            def get_bounds(p, obj=obj, bounds=bounds):
                check(p.reg('ECX') == obj, 'renderable sphere callback receiver')
                self.calls.append(('bounds', obj))
                p.fixture_return(eax=bounds)

            def draw(p, obj=obj):
                check(p.reg('ECX') == obj and p.uint(p.reg('ESP') + 4) == self.camera and
                      p.uint(p.reg('ESP') + 8) == self.node + 0xb4,
                      'direct renderable slot9 gets camera then adjusted support pointer')
                self.calls.append(('draw', obj))
                p.fixture_return(8, eax=0)  # native direct dispatch ignores this result

            p.seams[bounds_entry], p.seams[draw_entry] = get_bounds, draw
        for off, value in ((0xbc, storage), (0xc0, storage + len(spheres) * 4),
                           (0xc4, storage + len(spheres) * 4)):
            p.put_uint(self.node + off, value)

    def close(self):
        # Borrowed literal geometry fixtures must not enter native intrusive
        # teardown. They were never claimed to be constructed/owned renderables.
        for offset in (0xbc, 0xc0, 0xc4):
            self.p.put_uint(self.node + offset, 0)
        self.call(0x4255d0, this=self.node, args=(1,))
        super().close()


def world_and_lazy_matrices():
    f = RuntimeFixture()
    p, n = f.p, f.node
    p.put_floats(n + 0x20, (10., 20., 30.))
    p.put_floats(n + 0x30, (2., -3., 4.))
    p.put_floats(n + 0x40, (0., 1., 0., -1., 0., 0., 0., 0., 1.))
    p.put_floats(n + 0xc8, (1., 2., 3., 2.))
    p.put_uint(n + 0xb0, p.uint(n + 0xb0) | 1)
    f.call(0x4250f0, this=n, args=(0,))
    check(near(p.floats(n + 0xd8, 4), (16., 22., 42., 8.)),
          'world sphere uses PRS and max absolute scale, including negative scale')
    check(near(p.floats(n + 0x1b8, 3), (.5, -1 / 3, .25)), 'inverse world scales refreshed')
    check(p.uint(n + 0x134) == 1 and p.floats(n + 0x138, 16) == IDENTITY4,
          'world update invalidates, but does not eagerly compute render matrices')
    p.put_uint(f.renderer + 0xc9c8, 0x12345678)
    f.matrix_result = 0
    check(f.call(0x4248d0, this=n + 0xb4) & 255 == 0, 'matrix backend failure propagated')
    check(p.uint(n + 0x134) == 0 and p.uint(f.renderer + 0xc9c8) == 0x12345678,
          'lazy matrices commit before backend failure, renderer sphere copy does not')
    check(p.uint(f.renderer + 0xc190) == n + 0xf0,
          'renderer light-cache pointer published even before matrix result')
    world = p.floats(n + 0x138, 16)
    inverse = p.floats(n + 0x178, 16)
    check(near(world, (0., 2., 0., 0., 3., 0., 0., 0., 0., 0., 4., 0., 10., 20., 30., 1.)),
          'original lazy world matrix scales orientation rows')
    product = tuple(sum(world[r * 4 + k] * inverse[k * 4 + c] for k in range(4))
                    for r in range(4) for c in range(4))
    check(near(product, IDENTITY4), 'original inverse builder cancels signed-scale affine transform')
    f.matrix_result = 7
    check(f.call(0x4248d0, this=n + 0xb4) & 255 == 1 and
          near(p.floats(f.renderer + 0xc9c8, 4), (16., 22., 42., 8.)),
          'nonzero backend result normalized; world sphere copied only on success')
    p.put_uint(f.renderer + 0xc190, 0x12345678)
    p.mu.mem_write(f.renderer + 0xc9c4, b'\1')
    f.call(0x4248d0, this=n + 0xb4)
    check(p.uint(f.renderer + 0xc190) == 0x12345678, 'busy renderer retains previous light-cache owner')
    p.mu.mem_write(f.renderer + 0xc9c4, b'\0')
    # Clean calls still refresh reciprocal scale, but preserve sphere/matrices.
    p.put_floats(n + 0x1b8, (99., 99., 99.))
    p.put_floats(n + 0xc8, (50., 60., 70., 80.))
    f.call(0x4250f0, this=n, args=(0,))
    check(near(p.floats(n + 0x1b8, 3), (.5, -1 / 3, .25)) and
          near(p.floats(n + 0xd8, 4), (16., 22., 42., 8.)) and p.uint(n + 0x134) == 0,
          'clean world call refreshes reciprocal but not sphere or matrix dirty state')
    f.install_renderables(((1., 2., 3., 2.),))
    f.call(0x4250f0, this=n, args=(2,))
    check(near(p.floats(n + 0xd8, 4), (16., 22., 42., 4.)) and p.uint(n + 0x134) == 0,
          'bounds-only dirty uses cached matrix X radius, without forcing PRS/max-scale pass')
    f.call(0x4250f0, this=n, args=(1,))
    check(near(p.floats(n + 0xd8, 4), (16., 22., 42., 8.)),
          'later transform-dirty replaces bounds-only radius with max-scale radius')
    p.put_uint(n + 0xb0, 0x170a00)
    p.put_uint(n + 0x134, 0)
    f.call(0x4250f0, this=n, args=(0,))
    check(p.uint(n + 0xb0) & 1 and p.uint(n + 0x134) == 0,
          'clean billboard arms NEXT transform pass after capture, not current matrix cache')
    f.call(0x4250f0, this=n, args=(0,))
    check(p.uint(n + 0xb0) & 1 and p.uint(n + 0x134) == 1,
          'following billboard pass consumes captured dirty and rearms next frame')
    f.close()


def bounds_and_cull():
    f = RuntimeFixture()
    p, n = f.p, f.node
    # Two overlapping equal spheres give an exact, unambiguous union here.
    f.install_renderables(((0., 0., 0., 2.), (2., 0., 0., 2.)))
    p.put_uint(n + 0xb0, p.uint(n + 0xb0) | 3)
    f.call(0x4250f0, this=n, args=(0,))
    check(near(p.floats(n + 0xc8, 4), (1., 0., 0., 3.)) and
          near(p.floats(n + 0xd8, 4), (1., 0., 0., 3.)),
          'native bounds-dirty aggregation follows sphere callbacks then world conversion')
    check([item[1] for item in f.calls if item[0] == 'bounds'] == f.renderables,
          'all renderables contribute in stored order')
    # Explicit six-plane camera fixture; no original camera/frustum initialization claim.
    for i in range(6):
        p.put_floats(f.camera + 0x1c4 + i * 16, (1., 0., 0., -100.))
    check(f.call(0x424840, this=n, args=(f.camera,)) & 255 == 0, 'all six inside planes retained')
    for i in range(6):
        p.put_floats(f.camera + 0x1c4 + i * 16, (1., 0., 0., 4.))
        check(f.call(0x424840, this=n, args=(f.camera,)) & 255 == 0,
              'exact tangent distance equals negative radius and is not culled')
        p.put_floats(f.camera + 0x1c4 + i * 16, (1., 0., 0., 4.25))
        check(f.call(0x424840, this=n, args=(f.camera,)) & 255 == 1,
              'each of six plane positions independently rejects an outside sphere')
        p.put_floats(f.camera + 0x1c4 + i * 16, (1., 0., 0., -100.))
    p.put_floats(f.camera + 0x1c4, (1., 0., 0., 100.))
    p.mu.mem_write(n + 0x130, b'\1')
    check(f.call(0x424840, this=n, args=(f.camera,)) & 255 == 0,
          'byte130 bypasses frustum rejection')
    p.mu.mem_write(n + 0x130, b'\0')
    threshold = p.floats(0x6dca98, 1)[0]
    check(threshold == 0.0010000000474974513, 'exact binary cull radius threshold')
    for radius in (0., -1., threshold):
        p.put_floats(n + 0xe4, (radius,))
        check(f.call(0x424840, this=n, args=(f.camera,)) & 255 == 0,
              'nonpositive/tiny radius bypasses culling, not automatic disappearance')
    f.close()


def draw_gates():
    f = RuntimeFixture()
    p, n = f.p, f.node
    f.install_renderables(((0., 0., 0., 1.), (0., 0., 0., 1.)))
    p.put_floats(n + 0xd8, (0., 0., 0., 1.))
    p.put_floats(f.camera + 0x1c4, (1., 0., 0., 100.))
    p.put_uint(n + 0xb0, p.uint(n + 0xb0) & ~0x200)
    check(f.call(0x424b60, this=n + 0xb4, args=(f.camera, 0)) & 255 == 1 and not f.calls,
          'disabled node succeeds without cull/device/draw calls')
    p.put_uint(n + 0xb0, p.uint(n + 0xb0) | 0x200)
    check(f.call(0x424b60, this=n + 0xb4, args=(f.camera, 0)) & 255 == 1 and not f.calls,
          'culled node succeeds without matrix or draw work')
    f.matrix_result = 0
    check(f.call(0x424b60, this=n + 0xb4, args=(f.camera, 1)) & 255 == 0 and
          f.calls == [('matrix',)], 'forced visibility bypasses cull but not matrix failure')
    f.calls.clear()
    f.matrix_result = 1
    check(f.call(0x424b60, this=n + 0xb4, args=(f.camera, 1)) & 255 == 1 and
          f.calls == [('matrix',), *[('draw', obj) for obj in f.renderables]],
          'direct path dispatches all renderables and ignores their false return values')
    f.calls.clear()
    p.mu.mem_write(f.renderer + 0xc050, b'\1')
    f.queued_results = [1, 0]
    check(f.call(0x424b60, this=n + 0xb4, args=(f.camera, 1)) & 255 == 0 and
          f.calls == [('queue', obj) for obj in f.renderables],
          'queued path skips immediate matrix setup and propagates submission failure')
    f.calls.clear()
    f.queued_results = [0]
    check(f.call(0x424b60, this=n + 0xb4, args=(f.camera, 1)) & 255 == 0 and
          f.calls == [('queue', f.renderables[0])], 'first queued failure stops remaining renderables')
    f.close()


def main():
    world_and_lazy_matrices()
    bounds_and_cull()
    draw_gates()
    print(f'PASS {checks}/{checks}: original PC render-node world/bounds/matrices/cull/draw gates')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
