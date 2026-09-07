#!/usr/bin/env python3
"""Original empty PC actor construction/destruction with a synthetic registry root."""
from pathlib import Path
import sys
import probe_pc_animation_lifecycle as lifetime
from probe_pc_animation_lifecycle import LifetimeFixture, check
from pc_instruction_emulator import run_bounded


def main():
    fixture = LifetimeFixture()
    p = fixture.p
    # Only the registry storage root is synthetic. Controller registration and
    # removal execute original instructions, not callback replacements.
    manager = p.allocate(0x60)
    p.put_uint(0x75f880, manager)
    actor = fixture.call(0x5a3620)
    check(fixture.allocations[actor] == 0x54, 'exact actor factory allocation')
    check(p.uint(actor) == 0x703f80, 'original actor vtable')
    check(p.uint(manager + 0x24) == actor and p.uint(manager + 0x28) == actor,
          'controller constructor registers first/last node')
    check(p.uint(actor + 0x14) == 0 and p.uint(actor + 0x18) == 0, 'empty intrusive links')
    for offset in (0x10, 0x1c, 0x24):
        check(bytes(p.mu.mem_read(actor + offset, 1)) == b'\x01', 'enabled/apply/advance defaults')
    check(p.floats(actor + 0x20, 1)[0] == 1, 'actor time multiplier default')
    check(p.uint(actor + 0x2c) == 40, 'default playback capacity global')
    states = p.uint(actor + 0x28)
    check(fixture.allocations[states] == 40 * 0x60, 'exact playback-array bytes')
    for index in range(40):
        expected = bytearray(0x60)
        expected[0x44:0x48] = index.to_bytes(4, 'little')
        check(bytes(p.mu.mem_read(states + index * 0x60, 0x60)) == expected,
              'final memset state and slot index, not intermediate defaults')
    check(p.uint(actor + 0x4c) == 0xcccccccc and p.uint(actor + 0x50) == 0xcccccccc,
          'unwritten actor tail is not fabricated zero default')
    fixture.call(0x5a35a0, this=actor, args=(1,))
    check(p.uint(manager + 0x24) == 0 and p.uint(manager + 0x28) == 0,
          'controller destructor removes last registry node')
    check(set(fixture.freed) == set(fixture.allocations), 'all three owned allocations released')
    print(f'PASS {lifetime.checks}/{lifetime.checks}: original empty actor lifecycle')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
