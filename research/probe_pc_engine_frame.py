#!/usr/bin/env python3
"""Bounded original PC app->engine-update->animation dispatch evidence.

Engine/app storage and unrelated manager implementations are explicit seams.
Original spEngineCore constructor is NOT claimed: its protected route exceeded
the bounded scout. No Windows API is forwarded; foreground/owner/Sleep are data
fixtures. Real SAN/actor/node/animation manager code executes in the update.
"""
from pathlib import Path
import sys

from compare_pc_actor_binding import ScenarioFixture
from pc_instruction_emulator import run_bounded

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class FrameFixture(ScenarioFixture):
    def __init__(self):
        super().__init__()
        p = self.p
        self.stages = []
        self.delta = .25
        self.foreground = 0x101
        self.owner = 0
        self.graphics_result = 1
        self.next_seam = 0x34040010
        p.mu.mem_map(0x34040000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        self.error = self.object({0x20: ('error', 16)}, 0x30)
        p.put_uint(0x755264, self.error)
        clock_table = p.allocate(0x20)
        p.put_uint(clock_table + 0x1c, self.seam('clock', 0, self.engine + 0x54))
        p.put_uint(self.engine + 0x54, clock_table)
        self.input = self.object({}, 0x20)
        input_table = p.allocate(8)
        p.put_uint(input_table + 4, self.seam('input', 0, self.input + 0x18))
        p.put_uint(self.input + 0x18, input_table)
        p.put_uint(0x755288, self.input)
        for global_address, call_address in ((0x75db70, 0x451830),
                                               (0x75db80, 0x45a0c0),
                                               (0x75db90, 0x45a7d0)):
            obj = self.object({}, 0x20)
            p.put_uint(global_address, obj)
            self.seam(f'global{global_address:08X}', 0, obj, address=call_address)
        p.put_uint(self.engine + 0x3c, self.manager)
        gui = self.object({}, 0x50)
        p.put_uint(self.engine + 0x44, gui)
        self.seam('field44', 0, gui, address=0x451f70)
        p.put_uint(self.engine + 0x40, self.object({0x28: ('field40', 0)}, 0x30))
        p.put_uint(0x75db84, self.object({0x2c: ('global0075DB84', 0)}, 0x30))
        self.seam('queue-flush', 0, None, address=0x413b40)
        # The actual PC async leaf update is a no-op. Keep lazy creation and
        # normal destruction original; no file request is issued.
        p.put_uint(0x75db94, 0)
        p.mu.hook_add(p.uc.UC_HOOK_CODE, self.observe_native)
        self.app = self.object({0x30: ('pre-update', 0)}, 0x84)
        p.put_uint(self.app + 0x24, 0x101)
        self.seam('graphics', 0, self.engine, address=0x41c460)
        for call, name, pop in ((0x4c2d82, 'foreground', 0),
                               (0x4c2d90, 'owner', 8), (0x4c2d9d, 'sleep', 4)):
            p.put_uint(p.uint(call + 2), self.seam(name, pop, None))

    def object(self, slots, size):
        p = self.p
        obj = p.allocate(size)
        table = p.allocate(max([0x20, *[offset + 4 for offset in slots]]))
        p.put_uint(obj, table)
        for offset, (name, pop) in slots.items():
            p.put_uint(table + offset, self.seam(name, pop, obj))
        return obj

    def seam(self, name, pop, owner, *, address=None):
        if address is None:
            address = self.next_seam
            self.next_seam += 0x10
            if self.next_seam >= 0x34041000:
                raise ValueError('bounded seam page exhausted')

        def invoke(p):
            if owner is not None:
                check(p.reg('ECX') == owner, f'{name} exact receiver')
            args = tuple(p.uint(p.reg('ESP') + 4 + i) for i in range(0, pop, 4))
            if name == 'error':
                check(args == (0, 0, 0, 0), 'error reset four zero arguments')
            elif name == 'clock':
                p.put_floats(self.engine + 0xb8, [self.delta])
            elif name == 'queue-flush':
                queue = p.reg('ECX') - self.engine
                check(queue in (0xcc, 0x110), 'only two original engine queues')
                caller_return = p.uint(p.reg('ESP'))
                prefix = 'actor-' if 0x5a1600 <= caller_return <= 0x5a3700 else ''
                self.stages.append(f'{prefix}flush{queue:X}')
                p.fixture_return()
                return
            elif name == 'owner':
                check(args == (self.foreground, 4), 'GetWindow foreground/GW_OWNER fixture')
            elif name == 'sleep':
                check(args == (1,), 'background throttle requests exactly1ms; never sleeps host')
            self.stages.append(name)
            result = {'foreground': self.foreground, 'owner': self.owner,
                      'graphics': self.graphics_result}.get(name, 0)
            p.fixture_return(pop, eax=result)

        self.p.seams[address] = invoke
        return address

    def observe_native(self, mu, address, size, _):
        p = self.p
        if address == 0x4535a0:
            check(p.reg('ECX') == self.manager, 'engine field3C is actual animation manager')
            self.stages.append('animation')
        elif address == 0x48eaa0 and p.reg('ECX') == p.uint(0x75db94):
            self.stages.append('async-noop')

    def close(self):
        p = self.p
        async_manager = p.uint(0x75db94)
        if async_manager:
            self.call(p.uint(p.uint(async_manager)), this=async_manager, args=(1,))
        super().close()


UPDATE_ORDER = ['error', 'clock', 'input', 'global0075DB70', 'global0075DB80',
                'animation', 'actor-flush110', 'field44', 'global0075DB90', 'field40',
                'global0075DB84', 'flushCC', 'flush110', 'async-noop']


def original_inline_timers():
    f = FrameFixture()
    p = f.p
    root, child = f.engine + 0x54, f.engine + 0x90
    # Exact inline type/stride is independently proved by the core destructor.
    # Original ctor stores source2C; child-list attachment remains an explicit
    # fixture until the protected core constructor/attachment helpers are closed.
    f.call(0x4506f0, this=root, args=(0,))
    f.call(0x4506f0, this=child, args=(root,))
    p.put_uint(root + 0x30, child)
    p.put_uint(root + 0x34, child)
    p.put_uint(0x74e050, 1)
    p.mu.mem_write(0x75f66c, b'\0')
    p.put_uint(0x75f670, 1000)
    f.call(0x450840, this=root)
    p.mu.hook_add(p.uc.UC_HOOK_CODE, lambda mu, a, size, _: f.stages.append('clock')
                 if a == 0x450750 and p.reg('ECX') == root else None)
    f.start(fade=0)
    for milliseconds, expected_sample in ((1250, .25), (1500, .5), (1750, .75)):
        p.put_uint(0x75f670, milliseconds)
        f.stages.clear()
        f.call(0x4c2d60, this=f.app)
        check(f.stages == ['pre-update', *UPDATE_ORDER, 'foreground', 'graphics'],
              'same app/update order with actual inline timer constructors/update')
        check(p.floats(f.engine + 0xb8, 1) == (.25,) and
              p.floats(f.state(0) + 0x34, 1) == (expected_sample,),
              'real child timer deltaB8 drives actual actor sampling')
    f.call(0x4506b0, this=child)
    f.call(0x4506b0, this=root)
    f.close()


def main():
    f = FrameFixture()
    p = f.p
    f.start(fade=0)
    initial_position = p.floats(f.nodes[0] + 0x20, 3)
    initial_frame = p.uint(f.manager + 0x10)
    f.stages.clear()
    check(f.call(0x41cd50, this=f.engine) & 255 == 1, 'original engine update returns true')
    check(f.stages == UPDATE_ORDER, f'complete13-stage update order, ignores dependency false returns: {f.stages}')
    check(p.uint(f.manager + 0x10) == initial_frame + 1 and
          p.floats(f.state(0) + 0x34, 1) == (.25,), 'actual animation actor consumes clock delta')
    check(p.floats(f.nodes[0] + 0x20, 3) != initial_position, 'real SAN keys affect actual node local position')
    async_manager = p.uint(0x75db94)
    check(f.allocations[async_manager] == 0x18 and p.uint(async_manager) == 0x729078,
          'lazy actual PC async leaf factory, not synthetic manager identity')
    for foreground, owner, tail in ((0x101, 0, ['foreground', 'graphics']),
                                    (0x202, 0x101, ['foreground', 'owner', 'graphics']),
                                    (0x202, 0x303, ['foreground', 'owner', 'sleep'])):
        f.foreground, f.owner = foreground, owner
        f.graphics_result = 0  # spPCApp still succeeds even if graphics fails
        f.stages.clear()
        before = p.uint(f.manager + 0x10)
        check(f.call(0x4c2d60, this=f.app) & 255 == 1, 'base PC app update always succeeds')
        check(f.stages == ['pre-update', *UPDATE_ORDER, *tail], 'foreground/owner/background exact ordering')
        check(p.uint(f.manager + 0x10) == before + 1, 'background still advances original animation manager')
        check(p.uint(0x75db94) == async_manager, 'subsequent frame reuses async singleton')
    f.close()
    original_inline_timers()
    print(f'PASS {checks}/{checks}: original PC app/engine/animation frame with explicit external seams')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
