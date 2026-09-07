#!/usr/bin/env python3
"""Original PC node constructor and required identity-matrix startup dependency.

Only a tiny original math initializer is executed, not whole CRT/Windows startup.
Node heap/list/base constructor and destructor remain original bounded code.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture

checks = 0
IDENTITY = (1., 0., 0., 0., 1., 0., 0., 0., 1.)


def check(value, label):
    global checks
    checks += 1
    if not value: raise AssertionError(label)


def main():
    f = LifetimeFixture()
    p = f.p
    check(p.floats(0x7600bc, 9) == (0.,) * 9, 'PE-only guest has no CRT matrix initialization')
    missing_startup = f.call(0x421e20)
    check(p.floats(missing_startup + 0x40, 9) == (0.,) * 9,
          'node constructor copies uninitialized global if startup prerequisite is omitted')
    f.call(0x422220, this=missing_startup, args=(1,))
    f.call(0x6d38e0)
    check(p.floats(0x7600bc, 9) == IDENTITY, 'original static initializer constructs identity matrix')
    node = f.call(0x421e20)
    check(f.allocations[node] == 0xb4 and p.uint(node) == 0x6dc4f4, 'native node allocation/vtable')
    check(p.floats(node + 0x20, 3) == (0., 0., 0.) and p.floats(node + 0x30, 3) == (1., 1., 1.), 'native local position/scale defaults')
    check(p.floats(node + 0x40, 9) == IDENTITY and p.floats(node + 0x8c, 9) == IDENTITY, 'native local/world identity after actual startup')
    check(p.floats(node + 0x74, 3) == (0., 0., 0.) and p.floats(node + 0x80, 3) == (1., 1., 1.), 'native cached world position/scale defaults')
    check(p.uint(node + 0xb0) == 0x70a00 and p.uint(node + 0x2c) == p.uint(node + 0x3c) == 0,
          'runtime flags/null parent and scene link')
    head = p.uint(node + 0x18)
    check(p.uint(head) == head and p.uint(head + 4) == head and p.uint(node + 0x1c) == 0,
          'original empty child list sentinel')
    check(all(p.uint(node + offset) == 0 for offset in (0x68, 0x6c, 0x70)), 'empty collision pointer vector')
    f.call(0x422220, this=node, args=(1,))
    # Distinct finite diagnostic values prove copying, not coincidental identity
    # stores. No transform/render is attempted on this deliberately nonphysical matrix.
    diagnostic = tuple(float(i + 1) for i in range(9))
    p.put_floats(0x7600bc, diagnostic)
    copied = f.call(0x421e20)
    check(p.floats(copied + 0x40, 9) == diagnostic and p.floats(copied + 0x8c, 9) == diagnostic,
          'both matrices copy the shared constant, rather than hard-code identity')
    f.call(0x422220, this=copied, args=(1,))
    f.call(0x6d38e0)
    check(p.floats(0x7600bc, 9) == IDENTITY, 'original initializer restores diagnostic global')
    check(set(f.freed) == set(f.allocations), 'all original node/list allocations released')
    print(f'PASS {checks}/{checks}: original PC node defaults and matrix startup dependency')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
