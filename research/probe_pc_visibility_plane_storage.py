#!/usr/bin/env python3
"""Original PC plane-vector storage/copy construction; no45E870/46C0F0 call.

Only the existing bounded allocation/free and nonthrowing SEH fixtures are used.
Explicit20-byte records are not a claim that the full manager ctor completed.
One fresh guest process per case,100k instructions/2s per call,30s per child.
"""
from pathlib import Path
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


def plane_words(values=(1., 2., 3., 4.), flag=1):
    return struct.unpack('<5I', struct.pack('<4fI', *values, 0xaabbcc00 | flag))


class PlaneFixture(LifetimeFixture):
    def record(self):
        obj = self.p.allocate(20)
        self.p.put_uint(obj, 0xabcddcba)  # unknown allocator-state word, not initialized
        self.p.put_uint(obj + 16, 77)   # outer active count deliberately inconsistent
        return obj

    def resize(self, obj, count, words=None):
        self.call(0x46b720, this=obj, args=(count, *(words or plane_words())))

    def state(self, obj):
        p = self.p
        begin, end, limit = self.words(obj, 4, 16)
        check(0 <= end - begin <= limit - begin <= 20 * 32 and (end - begin) % 20 == 0,
              'bounded logical range/capacity in real plane vector')
        return begin, (end - begin) // 20, (limit - begin) // 20

    def close_record(self, obj):
        p = self.p
        begin = p.uint(obj + 4)
        active = p.uint(obj + 16)
        self.call(0x45ea00, this=obj)
        check(self.words(obj, 0, 20) == [0xabcddcba, 0, 0, 0, active],
              'actual vector destructor zeros pointer triple only')
        check(not begin or begin in self.freed, 'actual destructor frees owned storage')
        check(set(self.allocations) == set(self.freed), 'all observed normal-run allocations freed')


def resize_reuse():
    f = PlaneFixture(); p = f.p; obj = f.record()
    f.resize(obj, 0)
    check(f.state(obj) == (0, 0, 0) and not f.requests, 'empty clear does not allocate')
    f.resize(obj, 3)
    begin, count, capacity = f.state(obj)
    check((count, capacity) == (3, 3) and f.requests[-1][1] == 60, 'initial growth requests exactly3 planes')
    for index in range(3):
        at = begin + index * 20
        check(p.floats(at, 4) == (1., 2., 3., 4.) and p.uint(at + 16) == 0xcccccc01,
              'value copy writes equation and enabled byte, not by-value padding')
    p.mu.mem_write(begin + 37, b'\x11\x22\x33')
    f.resize(obj, 1)
    check(f.state(obj) == (begin, 1, 3) and not f.freed, 'shrink retains capacity and storage')
    f.resize(obj, 3, plane_words((8., 9., 10., 11.), 7))
    check(f.state(obj) == (begin, 3, 3) and len(f.requests) == 1, 'regrow within capacity reuses buffer')
    check(p.floats(begin, 4) == (1., 2., 3., 4.) and p.floats(begin + 20, 4) == (8., 9., 10., 11.),
          'regrow preserves active prefix, fills only newly live records')
    check(bytes(p.mu.mem_read(begin + 36, 4)) == b'\x07\x11\x22\x33',
          'reused slot retains old padding, raw nonzero enabled7 is not normalized')
    before = bytes(p.mu.mem_read(begin, 60))
    f.resize(obj, 3, plane_words((99., 99., 99., 99.), 0))
    check(bytes(p.mu.mem_read(begin, 60)) == before, 'same-size resize is a no-op for live planes')
    f.resize(obj, 0)
    check(f.state(obj) == (begin, 0, 3) and p.uint(obj + 16) == 77,
          'clear retains capacity and does not recompute outer activeCount')
    f.close_record(obj)


def growth_relocation():
    f = PlaneFixture(); p = f.p; obj = f.record()
    f.resize(obj, 3)
    old = p.uint(obj + 4)
    p.mu.mem_write(old + 17, b'\x11\x22\x33')
    p.put_uint(old, 0x7fc12345)  # NaN payload copy, no arithmetic/normalization
    f.resize(obj, 4, plane_words((4., 3., 2., 1.), 0))
    begin, count, capacity = f.state(obj)
    check(begin != old and old in f.freed and count == 4, 'growth relocates live prefix and releases old allocation')
    check(capacity == 4 and f.requests[-1][1] == 80, 'observed3-to4 growth requests exactly4 planes, not geometric doubling')
    check(p.uint(begin) == 0x7fc12345 and p.uint(begin + 16) == 0xcccccc01,
          'relocation preserves raw float bits/flag but does not copy source padding')
    check(p.floats(begin + 60, 4) == (4., 3., 2., 1.) and p.uint(begin + 76) == 0xcccccc00,
          'new suffix receives fill value and disabled byte')
    check(p.uint(obj + 16) == 77, 'growth still does not repair outer count')
    f.resize(obj, 5)
    begin, count, capacity = f.state(obj)
    check((count, capacity) == (5, 6) and f.requests[-1][1] == 120,
          '4-to5 grows capacity to6: max(requested,oldCapacity+floor(oldCapacity/2))')
    f.resize(obj, 0)
    f.resize(obj, 7)
    check(f.state(obj)[1:] == (7, 9) and f.requests[-1][1] == 180,
          'growth after clear uses retained capacity6, not old logical size0')
    f.close_record(obj)


def enabled_all():
    f = PlaneFixture(); p = f.p; obj = f.record()
    f.call(0x46adc0, this=obj, args=(1,))
    check(f.words(obj, 4, 20) == [0, 0, 0, 0], 'empty enable recomputes count0 without allocation')
    f.resize(obj, 4)
    begin = p.uint(obj + 4)
    for index in range(4):
        p.put_uint(begin + index * 20, 0x3f800000 + index)
        p.mu.mem_write(begin + index * 20 + 17, b'\x11\x22\x33')
    before = bytes(p.mu.mem_read(begin, 80))
    for value in (0, 1, 7, 255, 256, 257):
        f.call(0x46adc0, this=obj, args=(value,))
        check(p.uint(obj + 16) == (4 if value & 255 else 0), 'active count uses low-byte truth only')
        expected = bytearray(before)
        for index in range(4):
            expected[index * 20 + 16] = value & 255
        check(bytes(p.mu.mem_read(begin, 80)) == expected,
              'all enabled bytes receive raw low byte; equations and padding unchanged')
        check(f.state(obj) == (begin, 4, 4), 'set-enabled-all never changes storage or logical size')
    f.close_record(obj)


def copy_construction():
    f = PlaneFixture(); p = f.p
    for size in (0, 1, 3):
        source = f.record()
        f.resize(source, 6)
        f.resize(source, size)
        begin = p.uint(source + 4)
        if size:
            p.put_uint(begin, 0x7fc12345)
            p.mu.mem_write(begin + 16, b'\x07\x11\x22\x33')
        dest = p.allocate(20); p.mu.mem_write(dest, b'\xdd' * 20)
        result = f.call(0x45e530, this=dest, args=(source,))
        check(result == dest and 0x45e870 not in p.visits, 'actual copy-constructor returns this without assignment45E870')
        target, count, capacity = f.state(dest)
        check(count == capacity == size and (bool(target) == bool(size)),
              'copy constructor allocates exact logical size, not retained source capacity6')
        check(p.uint(dest) == p.uint(dest + 16) == 0xdddddddd,
              'copy constructor leaves destination allocator00 and outer activeCount10 untouched')
        if size:
            check(target != begin and p.uint(target) == 0x7fc12345 and p.uint(target + 16) == 0xcccccc07,
                  'deep value copy retains raw equation/flag, omits padding')
            p.put_uint(begin, 0)
            check(p.uint(target) == 0x7fc12345, 'source mutation cannot change separately owned copy')
        f.call(0x45ea00, this=source); f.call(0x45ea00, this=dest)
    check(set(f.allocations) == set(f.freed), 'all copy constructor allocations released')


def outer_stack_append():
    f = PlaneFixture(); p = f.p; source = f.record()
    f.resize(source, 3); f.resize(source, 2); p.put_uint(source + 16, 1)
    stack = p.allocate(16); p.put_uint(stack, 0xcafebabe)
    for count in range(1, 8):
        # Seventh append aliases an existing element while the outer vector grows.
        argument = p.uint(stack + 4) if count == 7 else source
        p.run(0x46c350, this=stack, args=(argument,), stop_at=0x45e870)
        check(p.reg('EIP') != 0x45e870 and 0x45e870 not in p.visits,
              'native append completes without entering known unresolved assignment boundary')
        begin, end, limit = f.words(stack, 4, 16)
        check((end - begin) // 20 == count and (limit - begin) // 20 == (1, 2, 3, 4, 6, 6, 9)[count - 1],
              'outer plane-stack append uses same floor1.5 capacity growth and spare-slot path')
        buffers = []
        for index in range(count):
            element = begin + index * 20
            buffer, size, capacity = f.state(element)
            buffers.append(buffer)
            check((size, capacity) == (2, 2) and p.uint(element + 16) == 1,
                  'outer copy construction copies activeCount separately and logical planes deeply')
            check(p.floats(buffer, 4) == (1., 2., 3., 4.), 'all appended/relocated plane equations preserved')
        check(len(set(buffers)) == count and p.uint(source + 4) not in buffers,
              'every nested set has independent owned plane storage, including aliased append')
        check(p.uint(stack) == 0xcafebabe, 'outer allocator word untouched')
    # Explicit surrounding ownership teardown, not proof of the full manager dtor.
    for element in range(p.uint(stack + 4), p.uint(stack + 8), 20):
        f.call(0x45ea00, this=element)
    p.run(0x412420, args=(p.uint(stack + 4),), callee_pop=False)
    f.call(0x45ea00, this=source)
    check(set(f.allocations) == set(f.freed), 'temporary deep copies, replaced arrays and all nested live allocations freed')


CASES = {'resize': resize_reuse, 'growth': growth_relocation, 'enabled': enabled_all,
         'copy': copy_construction, 'stack': outer_stack_append}


def main(case):
    CASES[case]()
    print(f'PASS {checks}/{checks}: PC visibility plane storage {case}')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(sys.argv[2]))
    for case in sys.argv[1:] or CASES:
        result = run_bounded(Path(__file__), (case,))
        if result:
            raise SystemExit(result)
