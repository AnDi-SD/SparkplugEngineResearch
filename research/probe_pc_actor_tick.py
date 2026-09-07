#!/usr/bin/env python3
"""Original PC actor tick with explicit CRT, event and controller boundary fixtures."""
from pathlib import Path
import math
import struct
import sys
from pc_instruction_emulator import PcInstructions, run_bounded
from probe_pc_animation_lifecycle import x87_value

checks = 0


def check(value, message):
    global checks
    checks += 1
    if not value:
        raise AssertionError(message)


def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def pop_x87(p):
    value = x87_value(p)
    top = (p.reg('FPSW') >> 11) & 7
    p.set_reg('FPTAG', p.reg('FPTAG') | (3 << (top * 2)))
    p.set_reg('FPSW', (p.reg('FPSW') & ~0x3800) | (((top + 1) & 7) << 11))
    return value


class ActorFixture:
    def __init__(self):
        self.p = p = PcInstructions()
        self.actor = p.allocate(0x54)
        self.animation = p.allocate(0x84)
        self.state = p.allocate(0x60)
        self.engine = p.allocate(0x160)
        self.debug = p.allocate(0x38)
        self.controller = p.allocate(0x18)
        self.evaluator = p.allocate(0x78)
        self.controller_vt = p.allocate(0x30)
        self.controller_array = p.allocate(4)
        self.map_sentinel = p.allocate(0x18)
        self.tags = p.allocate(4 * 8)
        self.tag_objects = p.allocate(0x1c * 8)
        p.mu.mem_map(0x34000000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        # Only the unbound guest IAT data slot is replaced; no original code bytes.
        floor_iat = p.uint(0x5a241c + 2)
        p.put_uint(floor_iat, 0x34000010)
        p.seams[0x34000010] = self.floor
        p.seams[0x60dd44] = self.fmod
        p.seams[0x40fa10] = self.queue_event
        p.seams[0x413b40] = self.flush
        p.seams[0x5a1c10] = self.rebind
        p.seams[0x34000020] = self.get_eval
        p.seams[0x34000030] = self.apply_direct
        p.seams[0x34000040] = self.apply_blended
        p.seams[0x34000050] = self.loop_callback
        p.put_uint(0x755274, self.engine)
        p.put_uint(0x75526c, self.debug)
        for offset, target in ((0x28, 0x34000020), (0x1c, 0x34000030), (0x20, 0x34000040)):
            p.put_uint(self.controller_vt + offset, target)
        p.put_uint(self.controller, self.controller_vt)
        p.put_uint(self.controller_array, self.controller)
        p.put_uint(self.evaluator + 0x18, self.state)
        p.put_uint(self.map_sentinel, self.map_sentinel)
        p.mu.mem_write(self.map_sentinel + 0x15, b'\x01')
        self.events = []

    def floor(self, p):
        value = struct.unpack('<d', p.mu.mem_read(p.reg('ESP') + 4, 8))[0]
        p.fixture_push_x87(float(math.floor(value)))
        p.fixture_return()

    def fmod(self, p):
        divisor = pop_x87(p)
        dividend = pop_x87(p)
        p.fixture_push_x87(math.fmod(dividend, divisor))
        p.fixture_return()

    def queue_event(self, p):
        sp = p.reg('ESP')
        code, queue, delay, state, payload = (p.uint(sp + 4 * i) for i in range(1, 6))
        check(p.reg('ECX') == self.actor and state == self.state, 'event owner/playback ABI')
        check(queue == self.engine + 0x110 and delay == 0xffffffff, 'event queue/delay ABI')
        self.events.append(['event', code, p.uint(payload + 0x18) if code == 11 else 'animation'])
        p.fixture_return(20)

    def flush(self, p):
        check(p.reg('ECX') == self.engine + 0x110, 'event flush receiver')
        self.events.append(['flush'])
        p.fixture_return()

    def rebind(self, p):
        check(p.uint(p.reg('ESP') + 4) == 0xffffffff, 'all-input rebind argument')
        self.events.append(['rebind'])
        p.fixture_return(4)

    def get_eval(self, p):
        self.events.append(['get-evaluator'])
        p.fixture_return(eax=self.evaluator)

    def apply_direct(self, p):
        self.events.append(['direct', *p.floats(p.reg('ESP') + 4, 1)])
        p.fixture_return(4)

    def apply_blended(self, p):
        self.events.append(['blend', *p.floats(p.reg('ESP') + 4, 2)])
        p.fixture_return(8)

    def loop_callback(self, p):
        check(p.uint(p.reg('ESP') + 4) == self.actor
              and p.uint(p.reg('ESP') + 8) == self.state, 'cdecl loop callback ABI')
        self.events.append(['callback'])
        p.fixture_return()

    def prepare(self, *, mode=1, progress=0, reverse=False, duration=4, sample=None,
                weight=1, fade=0, fade_rate=2, fade_out_rate=2, threshold=3,
                elapsed=0, transition=0, applies=True, advance=True, running=True,
                uses=1, multiplier=1, actor_multiplier=1, tags=(), callback=False,
                stop_after_fade=False):
        p = self.p
        p.mu.mem_write(self.actor, bytes(0x54))
        p.mu.mem_write(self.animation, bytes(0x84))
        p.mu.mem_write(self.state, bytes(0x60))
        p.mu.mem_write(self.actor + 0x1c, bytes([applies]))
        p.mu.mem_write(self.actor + 0x24, bytes([advance]))
        p.put_floats(self.actor + 0x20, [actor_multiplier])
        p.put_uint(self.actor + 0x28, self.state)
        p.put_uint(self.actor + 0x2c, 1)
        p.put_uint(self.actor + 0x34, self.map_sentinel)
        p.put_uint(self.actor + 0x40, self.controller_array)
        p.put_uint(self.actor + 0x44, self.controller_array + 4)
        p.put_uint(self.actor + 0x48, self.controller_array + 4)
        p.put_floats(self.animation + 0x14, [duration])
        check(len(tags) <= 8 and list(tags) == sorted(tags), 'bounded sorted tag fixture')
        for i, time in enumerate(tags):
            tag = self.tag_objects + i * 0x1c
            p.put_uint(self.tags + i * 4, tag)
            p.put_floats(tag + 0x14, [time])
            p.put_uint(tag + 0x18, i)
        if tags:
            p.put_uint(self.animation + 0x2c, self.tags)
            p.put_uint(self.animation + 0x30, self.tags + len(tags) * 4)
        p.put_uint(self.state, self.animation)
        p.put_uint(self.state + 4, mode)
        p.mu.mem_write(self.state + 8, bytes([reverse]))
        p.put_floats(self.state + 0xc, [weight])
        p.put_uint(self.state + 0x10, fade)
        p.put_floats(self.state + 0x18, [fade_rate])
        p.put_floats(self.state + 0x20, [fade_out_rate, transition])
        p.put_uint(self.state + 0x28, 0x34000050 if callback else 0)
        p.put_floats(self.state + 0x30, [multiplier, progress * duration if sample is None else sample])
        p.mu.mem_write(self.state + 0x3c, bytes([stop_after_fade]))
        p.put_uint(self.state + 0x48, uses)
        p.mu.mem_write(self.state + 0x4c, bytes([running]))
        p.put_floats(self.state + 0x54, [progress, threshold, elapsed])
        self.events.clear()

    def tick(self, delta):
        p = self.p
        p.run(0x5a2380, this=self.actor, args=(bits(delta),))
        return {'sample': p.floats(self.state + 0x34, 1)[0],
                'progress': p.floats(self.state + 0x54, 1)[0],
                'elapsed': p.floats(self.state + 0x5c, 1)[0],
                'transition': p.floats(self.state + 0x24, 1)[0],
                'weight': p.floats(self.state + 0xc, 1)[0],
                'fade': p.uint(self.state + 0x10), 'status': p.uint(self.state + 0x40),
                'uses': p.uint(self.state + 0x48),
                'running': bytes(p.mu.mem_read(self.state + 0x4c, 1))[0],
                'events': list(self.events)}


def main():
    fixture = ActorFixture()
    for mode in range(4):
        for progress, delta in ((0, 1), (.75, 1), (.75, 2), (1.75, 2), (.25, -2)):
            fixture.prepare(mode=mode, progress=progress, tags=(0, 1, 2, 3, 4), callback=True)
            print('TICK', mode, progress, delta, fixture.tick(delta), flush=True)
    print(f'PASS {checks}/{checks}: original actor tick scout boundary checks')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
