#!/usr/bin/env python3
"""Original PC spTaskTimer state, source-clock and child-update behavior.

Global hardware timer refresh is an explicit seam; no host timing/OS API is
called. Optional child-list edges are literal fixtures, not recovered Attach.
Original constructor/factory/clone/destructor and four timer operations execute.
"""
from pathlib import Path
import math
import struct
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


class TimerFixture(LifetimeFixture):
    def __init__(self):
        super().__init__()
        self.timers = []
        self.refreshes = []
        self.clone_pairs = []
        p = self.p
        p.mu.mem_write(0x75f66c, b'\0')
        p.put_uint(0x74e050, 1)
        p.put_uint(0x75f670, 0)
        for address in (0x6be2f0, 0x6be2d0):
            def refresh(p, address=address):
                check(p.reg('ECX') == 0x75f65c, 'hardware timer refresh exact global receiver')
                self.refreshes.append(address)
                p.fixture_return()
            p.seams[address] = refresh
        self.clone_manager = p.allocate(0x20)
        p.put_uint(0x74e060, self.clone_manager)
        def clone_pair(p):
            check(p.reg('ECX') == self.clone_manager, 'explicit clone-manager boundary')
            self.clone_pairs.append((p.uint(p.reg('ESP') + 4), p.uint(p.reg('ESP') + 8)))
            p.fixture_return(8)
        p.seams[0x412f70] = clone_pair

    def create(self, source=None):
        if source is None:
            timer = self.call(0x450880)
        else:
            # Allocate through the same bounded allocator but call the actual
            # one-argument ctor, which stores source2C without attaching a child.
            timer = self.p.allocate(0x3c)
            self.allocations[timer] = 0x3c
            self.p.mu.mem_write(timer, b'\xcc' * 0x3c)
            self.call(0x4506f0, this=timer, args=(source,))
        self.timers.append(timer)
        return timer

    def time(self, ticks, divisor=1, refresh=False):
        self.p.put_uint(0x75f670, ticks)
        self.p.put_uint(0x74e050, divisor)
        self.p.mu.mem_write(0x75f66c, bytes([refresh]))
        self.refreshes.clear()

    def snapshot(self, timer):
        p = self.p
        return [p.uint(timer + 0x18) & 255, (p.uint(timer + 0x18) >> 8) & 255,
                p.uint(timer + 0x1c), p.uint(timer + 0x20), p.uint(timer + 0x24),
                p.floats(timer + 0x28, 1)[0]]

    def close(self):
        for timer in reversed(self.timers):
            self.call(0x450730, this=timer, args=(1,))
        check(set(self.freed) == set(self.allocations), 'all tracked timer allocations freed')


def lifecycle_and_clock():
    f = TimerFixture()
    p = f.p
    timer = f.create()
    check(f.allocations[timer] == 0x3c and p.uint(timer) == 0x6e6968,
          'exact original3C factory/vtable')
    check(f.snapshot(timer) == [0, 1, 0, 0, 0, 0.], 'factory paused relative-clock defaults')
    check(all(p.uint(timer + i) == 0 for i in (0x10, 0x14, 0x2c, 0x30, 0x34, 0x38)),
          'all timer link/opaque words zero by factory')
    check(p.uint(timer + 0x18) == 0xcccc0100, 'constructor leaves padding18+2/+3 untouched')
    f.time(1200)
    p.put_floats(timer + 0x28, [2.5])
    f.call(0x450750, this=timer)
    check(f.snapshot(timer) == [0, 1, 0, 0, 0, 0.], 'paused update zeroes only delta')
    f.call(0x450840, this=timer)
    check(f.snapshot(timer) == [1, 1, 0, 1200, 0, 0.], 'start records source timestamp but not current time')
    f.time(1450)
    f.call(0x450750, this=timer)
    check(f.snapshot(timer) == [1, 1, 250, 1200, 0, .25], 'relative update computes elapsed milliseconds/delta seconds')
    f.call(0x4506d0, this=timer)
    check(f.snapshot(timer) == [0, 1, 250, 1200, 250, .25], 'pause captures current, does not clear old delta')
    f.time(9000, refresh=True)
    f.call(0x450750, this=timer)
    check(f.snapshot(timer) == [0, 1, 250, 1200, 250, 0.] and not f.refreshes,
          'paused update avoids hardware refresh')
    f.call(0x450840, this=timer)
    check(f.refreshes == [0x6be2f0, 0x6be2d0] and p.uint(timer + 0x20) == 9000,
          'start hardware refresh order and new baseline')
    f.time(18700, divisor=2, refresh=True)
    f.call(0x450750, this=timer)
    check(f.snapshot(timer)[:5] == [1, 1, 600, 9000, 250] and
          p.floats(timer + 0x28, 1)[0] == f32(350 * f32(.001)), 'resume preserves accumulated paused time and divisor')
    check(f.refreshes == [0x6be2f0, 0x6be2d0], 'running source refresh order')
    f.call(0x4506e0, this=timer)
    check(f.snapshot(timer)[:5] == [0, 1, 0, 0, 0] and p.floats(timer + 0x28, 1)[0] != 0,
          'reset clears three integer timestamps/active, leaves delta until next update')
    p.mu.mem_write(timer + 0x19, b'\0')
    f.time(1000)
    f.call(0x450840, this=timer)
    f.time(1250)
    f.call(0x450750, this=timer)
    check(f.snapshot(timer) == [1, 0, 1250, 1000, 0, 1.25], 'absolute mode ignores start baseline for current time')
    for previous, now in ((0xffffff00, 0x10), (0x10, 0), (0, 0xffffffff), (123, 123)):
        p.put_uint(timer + 0x1c, previous)
        f.time(now)
        f.call(0x450750, this=timer)
        check(p.uint(timer + 0x1c) == now and p.floats(timer + 0x28, 1)[0] ==
              f32(((now - previous) & 0xffffffff) * f32(.001)), 'unsigned subtraction/wrap and float32 conversion')
    source = f.create(timer)
    check(p.uint(source + 0x2c) == timer and p.uint(timer + 0x30) == 0,
          'source-clock ctor does not append to source child list')
    p.mu.mem_write(source + 0x18, b'\1')
    p.put_uint(timer + 0x1c, 321)
    p.put_floats(timer + 0x28, [.625])
    f.time(5000, refresh=True)
    f.call(0x450750, this=source)
    check(f.snapshot(source)[2] == 321 and f.snapshot(source)[5] == .625 and not f.refreshes,
          'linked clock directly copies current/delta without source update or hardware read')
    p.put_uint(timer + 0xc, 0x1234)
    clone = f.call(0x450910, this=source)
    f.timers.append(clone)
    check(f.snapshot(clone) == [0, 1, 0, 0, 0, 0.] and p.uint(clone + 0x2c) == 0,
          'clone fresh runtime does not copy linked source/time/flags')
    check(f.clone_pairs == [(source, clone)], 'one original clone registration pair')
    f.close()


def children():
    f = TimerFixture()
    p = f.p
    root, child, grandchild, sibling = [f.create() for _ in range(4)]
    # Literal non-owning links only; actual attachment helper remains open.
    p.put_uint(root + 0x30, child)
    p.put_uint(root + 0x34, sibling)
    p.put_uint(child + 0x14, sibling)
    p.put_uint(sibling + 0x10, child)
    p.put_uint(child + 0x30, grandchild)
    p.put_uint(child + 0x34, grandchild)
    p.put_uint(child + 0x2c, root)
    p.put_uint(grandchild + 0x2c, child)
    p.put_uint(sibling + 0x2c, root)
    seen = []
    p.mu.hook_add(p.uc.UC_HOOK_CODE, lambda mu, a, size, _: seen.append(p.reg('ECX'))
                 if a == 0x450750 else None)
    f.time(1000)
    f.call(0x450840, this=root)
    f.time(1250)
    f.call(0x450750, this=root)
    check(seen == [root, child, grandchild, sibling], 'virtual child update recursively follows list preorder')
    check(all(f.snapshot(t)[0:2] == [1, 1] and f.snapshot(t)[2] == 250 and
              f.snapshot(t)[5] == .25 for t in (root, child, grandchild, sibling)),
          'root active/mode propagate to descendants, linked clocks copy updated parent')
    f.call(0x4506d0, this=root)
    check(p.uint(child + 0x18) & 255 == 1, 'pause does not recurse immediately')
    seen.clear()
    f.call(0x450750, this=root)
    check(seen == [root, child, grandchild, sibling] and
          all(f.snapshot(t)[0] == 0 and f.snapshot(t)[5] == 0 for t in (root, child, grandchild, sibling)),
          'paused update still traverses children and propagates paused state')
    before = bytes(p.mu.mem_read(child, 0x3c))
    f.call(0x450730, this=root, args=(1,))
    f.timers.remove(root)
    check(bytes(p.mu.mem_read(child, 0x3c)) == before and f.freed == [root],
          'root destructor does not own/delete/detach child timer links')
    f.close()


def main():
    lifecycle_and_clock()
    children()
    print(f'PASS {checks}/{checks}: original PC task-timer lifecycle, time math and explicit child links')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
