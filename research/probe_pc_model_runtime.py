#!/usr/bin/env python3
"""Original PC model lifetime/bounds/copy/render protocol in bounded guest memory.

Model methods are unmodified. Mesh/material payloads below are explicit borrowed
records with an external reference, not claims of GPU-resource construction.
Renderer leaves record calls; no Direct3D, Windows loader or game process runs.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_lights import LightSceneFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class ModelFixture(LightSceneFixture):
    def __init__(self):
        super().__init__()
        self.models = []
        p = self.p
        for record, identity, parent in ((0x75e030, 0x4fda4542, 0x7555f8),
                                         (0x760cf8, 0x763277db, 0x75e030)):
            p.put_uint(record, identity)
            p.put_uint(record + 0x48, parent)

    def model(self):
        obj = self.call(0x479ed0)
        self.models.append(obj)
        return obj

    def mesh_record(self, sphere=(1., 2., 3., 4.)):
        obj = self.p.allocate(0x60)
        self.p.put_uint(obj + 8, 1)  # external reference prevents borrowed dtor
        self.p.put_floats(obj + 0x18, sphere)
        self.p.put_floats(obj + 0x2c, (-1., -2., -3., 4., 5., 6.))
        return obj

    def close(self):
        for obj in reversed(self.models):
            self.call(0x479f90, this=obj, args=(1,))
        super().close()


def lifetime_bounds_copy():
    f = ModelFixture()
    p = f.p
    source, target = f.model(), f.model()
    check(f.allocations[source] == 0x60 and p.uint(source) == 0x6eaa58,
          'PC factory exact60 and complete model vtable')
    check(p.uint(source + 0x58) == 0 and p.uint(source + 0x5c) == 3,
          'PC null mesh and projection group3 now independently proved')
    check(p.uint(source + 0x18) == 0xcccccc01 and p.uint(source + 0x28) == 0xff000000,
          'alpha byte true with untouched padding; opaque28 exact constructor bits')
    check(all(p.uint(source + off) == 0 for off in
              (0x14, 0x1c, 0x20, 0x24, 0x2c, 0x30, 0x38, 0x3c, 0x40, 0x48, 0x4c, 0x50)),
          'PC inherited relationships, callbacks, empty vector ranges and mode defaults')
    check(p.uint(source + 0x34) == p.uint(source + 0x44) == 0xcccccccc and
          p.uint(source + 0x54) == 0xcccc0000, 'allocator words and padding not zeroed')
    check(f.call(0x408370, this=source, args=(0x4fda4542,)) & 255,
          'original RTTI traverses model to renderable')
    zero = f.call(0x479d20, this=source)
    check(zero == 0x7601ac and p.floats(zero, 4) == (0.,) * 4,
          'null model mesh returns shared zero sphere')
    minimum, maximum = p.allocate(12), p.allocate(12)
    p.put_floats(minimum, (9.,) * 3)
    p.put_floats(maximum, (9.,) * 3)
    f.call(0x479d40, this=source, args=(minimum, maximum))
    check(p.floats(minimum, 3) == p.floats(maximum, 3) == (0.,) * 3 and
          f.call(0x479da0, this=source) == 0, 'null bounds are zero and invalid')
    first, old = f.mesh_record(), f.mesh_record((9., 8., 7., 6.))
    f.call(0x479e20, this=source, args=(first,))
    f.call(0x479e20, this=target, args=(old,))
    check(p.uint(first + 8) == p.uint(old + 8) == 2, 'setter retains one mesh reference')
    p.put_uint(source + 0x14, 0x12345678)
    f.call(0x479e20, this=source, args=(first,))
    check(p.uint(first + 8) == 2 and p.uint(source + 0x14) == 0x12345678,
          'same-mesh setter keeps ref; PC classifier no-op does NOT zero runtime word')
    check(f.call(0x479d20, this=source) == first + 0x18 and
          f.call(0x479da0, this=source) == 0, 'sphere pointer independent of validity flag')
    p.mu.mem_write(first + 0x28, b'\x7f')
    check(f.call(0x479da0, this=source) == 1, 'nonzero mesh-valid byte normalized')
    f.call(0x479d40, this=source, args=(minimum, maximum))
    check(p.floats(minimum, 3) == (-1., -2., -3.) and
          p.floats(maximum, 3) == (4., 5., 6.), 'six mesh extent words copied verbatim')
    f.call(0x423bf0, this=source, args=(minimum, maximum))
    check(p.floats(minimum, 3) == (-3., -2., -1.) and
          p.floats(maximum, 3) == (5., 6., 7.),
          'base bounds slot builds center +/- radius through concrete model sphere getter')
    p.put_uint(source + 0x5c, 0x1234)
    p.put_uint(source + 0x28, 0x3f123456)
    p.put_uint(source + 0x2c, 0x12345678)  # never executed, copy exclusion marker
    p.put_uint(target + 0x14, 0x87654321)
    check(f.call(0x479e60, this=source, args=(target,)) & 255,
          'original inherited/model copy succeeds')
    check(p.uint(target + 0x58) == first and p.uint(first + 8) == 3 and p.uint(old + 8) == 1,
          'copy releases destination and shares source mesh instead of geometry clone')
    check(p.uint(target + 0x5c) == 0x1234 and p.uint(target + 0x28) == 0x3f123456 and
          p.uint(target + 0x2c) == 0 and p.uint(target + 0x14) == 0x87654321,
          'projection/opaque28 copied; callbacks and PC runtime mode are not copied/reset')
    p.seams[0x412f70] = lambda p: p.fixture_return(8)  # root clone-pair transaction seam
    clone = f.call(0x479f40, this=source)
    f.models.append(clone)
    check(clone in f.allocations and p.uint(clone + 0x58) == first and
          p.uint(first + 8) == 4 and p.uint(clone + 0x5c) == 0x1234 and
          p.uint(clone + 0x14) == 0, 'actual clone shares mesh with fresh default mode')
    f.call(0x479e20, this=source, args=(0,))
    check(p.uint(first + 8) == 3 and p.uint(source + 0x58) == 0,
          'null setter releases original relationship')
    f.close()
    check(p.uint(first + 8) == p.uint(old + 8) == 1,
          'actual model destructors return both borrowed meshes to external ref only')


def render_protocol():
    f = ModelFixture()
    p = f.p
    obj = f.model()
    mesh = f.mesh_record()
    f.call(0x479e20, this=obj, args=(mesh,))
    camera, support = p.allocate(4), p.allocate(4)
    table = p.allocate(0x80)
    p.put_uint(f.renderer + 0x18, table)
    p.mu.mem_map(0x340b0000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
    p.put_uint(table + 0x24, 0x340b0010)
    p.put_uint(table + 0x68, 0x340b0020)
    calls = []
    result = {'mesh': 1, 'fog': 0, 'pre': 1, 'post': 1}

    def device(p, name, expected):
        check(p.reg('ECX') == f.renderer + 0x18 and p.uint(p.reg('ESP') + 4) == expected,
              'original renderer secondary receiver and leaf argument')
        calls.append(name)
        p.fixture_return(4, eax=result[name])

    def direct(p, name):
        check([p.uint(p.reg('ESP') + i) for i in (4, 8, 12)] == [obj, camera, support],
              'original direct callback is cdecl(model,camera,support)')
        calls.append(name)
        p.fixture_return(eax=result[name])

    p.seams[0x340b0010] = lambda p: device(p, 'mesh', mesh)
    p.seams[0x340b0020] = lambda p: device(p, 'fog', 0)
    p.seams[0x340b0030] = lambda p: direct(p, 'pre')
    p.seams[0x340b0040] = lambda p: direct(p, 'post')
    p.put_uint(obj + 0x2c, 0x340b0030)
    p.put_uint(obj + 0x30, 0x340b0040)
    p.put_uint(f.renderer + 0xc9c0, 0x11223344)
    check(f.call(0x479dc0, this=obj, args=(camera, support)) & 255 == 1 and
          calls == ['pre', 'fog', 'mesh', 'post'], 'full original pre/mesh/post ordering; false fog ignored')
    check(p.uint(f.renderer + 0xc18c) == 0x11223344 and
          p.uint(f.renderer + 0xc194) == p.uint(obj + 0x28), 'fallback material and opaque28 cache publication')
    calls.clear()
    result['pre'] = 0
    check(f.call(0x479dc0, this=obj, args=(camera, support)) & 255 == 1 and calls == ['pre'],
          'false pre skips mesh/post but means successful model dispatch')
    calls.clear()
    result['pre'], result['mesh'] = 1, 0
    check(f.call(0x479dc0, this=obj, args=(camera, support)) & 255 == 0 and
          calls == ['pre', 'fog', 'mesh'], 'false mesh propagates and skips post')
    calls.clear()
    result['mesh'], result['post'] = 7, 0
    check(f.call(0x479dc0, this=obj, args=(camera, support)) & 255 == 0 and
          calls == ['pre', 'fog', 'mesh', 'post'], 'nonzero mesh reaches failing post')
    calls.clear()
    result['post'] = 7
    p.mu.mem_write(f.renderer + 0xc188, b'\1')
    p.put_uint(f.renderer + 0xc18c, 0x55667788)
    p.put_uint(f.renderer + 0xc194, 0x12345678)
    check(f.call(0x479dc0, this=obj, args=(camera, support)) & 255 == 1 and
          p.uint(f.renderer + 0xc18c) == 0x55667788 and
          p.uint(f.renderer + 0xc194) == 0x12345678, 'material override preserves both cached words')
    f.close()


def meshdata_constructor_state():
    f = ModelFixture()
    p = f.p
    mesh = f.call(0x41a270)
    check(f.allocations[mesh] == 0x58 and p.uint(mesh) == 0x6de8fc and
          p.uint(mesh + 0x14) == 0x6de8f4, 'actual PC MeshData factory exact58 and both vptrs')
    check(p.floats(mesh + 0x18, 4) == (0.,) * 4 and
          p.uint(mesh + 0x28) == 0xcccccc00 and
          all(p.uint(mesh + i) == 0xcccccccc for i in range(0x2c, 0x44, 4)),
          'mesh sphere zero/valid false, extent words and padding untouched')
    check(p.uint(mesh + 0x50) == p.uint(mesh + 0x54) == 0xcccccccc,
          'PC MeshData constructor does not initialize either owned buffer pointer')
    # Explicit cold-state fixture before destructor: this is NOT the native
    # Initialize/deep-copy path and does not convert uninitialized bytes into
    # a constructor guarantee. Never call through the poisoned owner pointers.
    p.put_uint(mesh + 0x50, 0)
    p.put_uint(mesh + 0x54, 0)
    f.call(p.uint(p.uint(mesh)), this=mesh, args=(1,))
    manager = p.uint(0x75db78)
    check(mesh in f.freed and manager in f.allocations and
          f.allocations[manager] == 0x30 and p.uint(manager) == 0x6e703c,
          'resource teardown lazily creates actual ResourceManager with exact PC30 allocation')
    f.call(p.uint(p.uint(manager)), this=manager, args=(1,))
    check(p.uint(0x75db78) == 0, 'actual empty resource manager destructor clears own singleton')
    f.close()


def main():
    lifetime_bounds_copy()
    render_protocol()
    meshdata_constructor_state()
    print(f'PASS {checks}/{checks}: original PC model lifetime/bounds/copy/render protocol')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
