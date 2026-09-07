#!/usr/bin/env python3
"""Original core default-scene/camera/inline timer setup, with explicit init seam.

Synthetic engine storage, real Scene/DXCamera factories and TaskTimer ctors.
Scene initialize45D850 is deliberately a result-only boundary: no GPU/resource
initialization is claimed. Error collection is guest-only, never dialog/OS.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_world import SceneFixture, scene_entries
from probe_pc_san_reader import cstring

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def setup(append=False, success=True):
    f = SceneFixture()
    p = f.p
    engine = p.uint(0x755274)
    root, child = engine + 0x54, engine + 0x90
    f.call(0x4506f0, this=root, args=(0,))
    f.call(0x4506f0, this=child, args=(0,))
    previous = 0
    if append:
        previous = f.call(0x450880)
        p.put_uint(root + 0x30, previous)
        p.put_uint(root + 0x34, previous)
        p.put_uint(root + 0x38, 1)
    calls = []

    def init(p):
        scene = p.reg('ECX')
        check(scene == p.uint(engine + 0x18) and f.allocations[scene] == 0x54,
              'actual scene factory published before initialization')
        f.scenes.append(scene)
        calls.append('scene-initialize')
        p.fixture_return(eax=int(success))

    p.seams[0x45d850] = init
    if not success:
        error_object, error_vtable = p.allocate(0x30), p.allocate(0x24)
        p.put_uint(error_object, error_vtable)
        p.put_uint(error_vtable + 0x1c, 0x34060010)
        p.mu.mem_map(0x34060000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        p.put_uint(0x755264, error_object)
        record = p.allocate(0x30)

        def create_error(p):
            args = [p.uint(p.reg('ESP') + 4 + i * 4) for i in range(5)]
            check(p.reg('ECX') == error_object and args[:3] == [0, 1, 1] and
                  cstring(p, args[3]).endswith(b'spEngineCore.cpp') and args[4] == 102,
                  'original error source/line/severity arguments')
            calls.append('error-create')
            p.fixture_return(20, eax=record)

        def message(p):
            check(p.reg('ECX') == record and cstring(p, p.uint(p.reg('ESP') + 4)) ==
                  b'Failed to init default scene', 'original error text')
            calls.append('error-text')
            p.fixture_return(4)

        def report(p):
            check(p.reg('ECX') == error_object and p.uint(p.reg('ESP') + 4) == record and
                  p.uint(p.reg('ESP') + 8) == 1, 'original error report arguments')
            calls.append('error-report')
            p.fixture_return(8)

        p.seams[0x40efa0] = create_error
        p.seams[0x416cb0] = message
        p.seams[0x34060010] = report
    result = f.call(0x41c300, this=engine) & 255
    check(result == int(success), 'core stage returns scene initialization result')
    scene = p.uint(engine + 0x18)
    check(scene_entries(p, p.uint(0x75db90)) == [scene], 'default scene uses native manager registry')
    if success:
        camera = p.uint(engine + 0x1c)
        check(f.allocations[camera] == 0x238 and p.uint(camera) == 0x6ef1e0,
              'actual spDXCamera exact238/vtable')
        check(p.uint(camera + 8) & 65535 == 1 and
              cstring(p, p.uint(camera + 0x10) + 9) == b'Default Camera',
              'default camera retained once and named')
        check(p.uint(camera + 0x2c) == p.uint(camera + 0x3c) == 0 and p.uint(engine + 0x24) == 0,
              'this stage does not attach or activate/register default camera')
        check(bytes(p.mu.mem_read(root + 0x18, 2)) == b'\1\1' and
              p.uint(child + 0x2c) == root, 'core activates relative root and wires child source')
        check(p.uint(root + 0x30) == (previous or child) and p.uint(root + 0x34) == child and
              p.uint(root + 0x38) == (2 if append else 1), 'native append proves timer head/tail/count')
        check(p.uint(child + 0x10) == previous and p.uint(child + 0x14) == 0,
              'native child previous/next sibling wiring')
        if append:
            check(p.uint(previous + 0x14) == child, 'existing tail points to newly appended timer')
        check(all(p.uint(root + offset) == 0 for offset in (0x1c, 0x20, 0x24, 0x28)),
              'timer wiring does not Start or populate timestamps/delta')
        check(calls == ['scene-initialize'], 'no failure diagnostics on successful scene init')
        f.call(0x4a91e0, this=camera, args=(1,))
        p.put_uint(engine + 0x1c, 0)
    else:
        check(calls == ['scene-initialize', 'error-create', 'error-text', 'error-report'],
              'failed init reports in order and returns without camera/timer wiring')
        check(p.uint(engine + 0x1c) == 0 and p.uint(child + 0x2c) == 0 and
              bytes(p.mu.mem_read(root + 0x18, 2)) == b'\0\1', 'failed scene stage leaves timer graph unwired')
        check(scene not in f.freed, 'failed core stage does not roll back created scene')
    f.call(0x4506b0, this=child)
    f.call(0x4506b0, this=root)
    if previous:
        f.call(0x450730, this=previous, args=(1,))
    f.close()


def main():
    setup()
    setup(append=True)
    setup(success=False)
    print(f'PASS {checks}/{checks}: original PC core scene/camera/timer initialization edge')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
