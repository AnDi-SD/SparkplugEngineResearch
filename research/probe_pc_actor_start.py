#!/usr/bin/env python3
"""Original actor start request, priority packing and guarded capacity scout."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_actor_binding import ActorBindingFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class StartFixture(ActorBindingFixture):
    def __init__(self,playback_capacity=None):
        super().__init__(playback_capacity)
        self.animation = self.load_animation()
        for track in self.tracks[:2]:
            self.discover(self.named_node(track['name']))
        self.request = self.p.allocate(0x38)
        self.flushes = 0
        self.p.seams[0x413b40] = self.flush
        self.empty_states = bytes(self.p.mu.mem_read(self.p.uint(self.actor+0x28), self.p.uint(self.actor+0x2c) * 0x60))
        self.empty_evaluators = {self.p.uint(c + 0x14): bytes(self.p.mu.mem_read(self.p.uint(c + 0x14), 0x78))
                                 for c in self.controllers()}

    def flush(self, p):
        check(p.reg('ECX') == self.engine + 0x110, 'start flush uses engine animation queue')
        self.flushes += 1
        p.fixture_return()

    def reset_start_fixture(self):
        self.p.mu.mem_write(self.state(0), self.empty_states)
        for address, data in self.empty_evaluators.items():
            self.p.mu.mem_write(address, data)
        self.events.clear()
        self.flushes = 0

    def params(self, *, animation=None, mode=1, reverse=False, fade=0, weight=.25,
               fade_in_duration=0, fade_in_rate=.5, fade_out_duration=0,
               fade_out_rate=.75, transition=0, callback=0, cookie=0,
               speed=1, initial_time=0):
        p, request = self.p, self.request
        p.mu.mem_write(request, bytes(0x38))
        for offset, value in ((0, animation or self.animation), (4, mode), (0x10, fade),
                              (0x28, callback), (0x2c, cookie)):
            p.put_uint(request + offset, value)
        p.mu.mem_write(request + 8, bytes([reverse]))
        for offset, value in ((0xc, weight), (0x14, fade_in_duration), (0x18, fade_in_rate),
                              (0x1c, fade_out_duration), (0x20, fade_out_rate),
                              (0x24, transition), (0x30, speed), (0x34, initial_time)):
            p.put_floats(request + offset, [value])
        return request

    def start(self, **kwargs):
        self.events.clear()
        self.flushes = 0
        return self.call(0x5a1e30, this=self.actor, args=(self.params(**kwargs),))


def request_fields_and_restart():
    f = StartFixture()
    p = f.p
    p.put_uint(f.animation + 0x18, 3)
    p.put_uint(f.manager + 0x10, 0x12345678)
    for fade, expected_weight, expected_status in ((0, 1, 1), (1, .25, 1), (2, 0, 0), (3, 1, 1), (4, 0, 1)):
        f.reset_start_fixture()
        result = f.start(fade=fade, reverse=True, fade_in_duration=2, fade_out_duration=.25,
                         transition=.75, speed=2, initial_time=.5, cookie=0x1234)
        check(result == 0, 'first free playback slot selected')
        state = f.state(0)
        check(p.floats(f.request + 0xc, 1)[0] == expected_weight and
              p.floats(state + 0xc, 1)[0] == expected_weight,
              'fade mode mutates request weight then initializes new-state weight')
        check(p.uint(state + 0x40) == expected_status and p.uint(state + 0x50) == 0x03345678,
              'initial status and animation-high8/frame-low24 packed priority')
        check(p.floats(state + 0x18, 1)[0] == .5 and p.floats(state + 0x20, 1)[0] == 4,
              'positive fade durations converted to reciprocal rates')
        check(p.floats(state + 0x58, 1)[0] == .75 and p.floats(state + 0x24, 1)[0] == .75,
              'fade threshold uses animation duration minus requested fade-out duration')
        check(p.uint(state + 0x2c) == 0x1234 and p.floats(state + 0x30, 1)[0] == 2 and
              p.floats(state + 0x54, 1)[0] == .5 and p.floats(state + 0x34, 1)[0] == 0,
              'cookie/speed/progress initialized but sample time remains old storage')
        check([event[0] for event in f.events] == ([2, 6, 4] if fade == 2 else [2, 4]) and f.flushes == 1,
              'start, optional fade-in, gained-binding events then one flush')
        check(f.events[0][3:] == (state, f.animation) and f.events[-1][3:] == (state, state),
              'start event payload is animation; binding event payload is playback state')
    # Restart an already-used animation: weight/sample/stop-after-fade are not
    # reset by start, although request weight still changes according to fade.
    p.put_floats(f.state(0) + 0xc, [.625])
    p.put_floats(f.state(0) + 0x34, [.875])
    p.mu.mem_write(f.state(0) + 0x3c, b'\1')
    result = f.start(fade=0, initial_time=.25, fade_in_duration=-1, fade_in_rate=7,
                     fade_out_duration=0, fade_out_rate=9)
    check(result == 0 and p.floats(f.request + 0xc, 1)[0] == 1 and
          p.floats(f.state(0) + 0xc, 1)[0] == .625, 'active-animation restart preserves current weight')
    check(p.floats(f.state(0) + 0x34, 1)[0] == .875 and
          bytes(p.mu.mem_read(f.state(0) + 0x3c, 1)) == b'\1', 'restart does not reset sample/stop-after-fade')
    check(p.floats(f.state(0) + 0x18, 1)[0] == 7 and p.floats(f.state(0) + 0x20, 1)[0] == 9,
          'nonpositive duration selects caller-provided rates')
    check([event[0] for event in f.events] == [2], 'restart still emits start, not duplicate gained event')
    f.close()


def third_candidate_stopped_before_binding():
    f = StartFixture()
    p = f.p
    second, third = f.load_animation(), f.load_animation()
    f.start(fade=2)
    f.start(animation=second, fade=2)
    before = f.snapshot_inputs()
    request = f.params(animation=third, fade=2)
    # Stop BEFORE the binder call, so the third insertion is never attempted.
    # Start has no SEH frame; earlier leaf calls have restored FS:0 normally.
    p.run(0x5a1e30, this=f.actor, args=(request,), stop_at=0x5a205b)
    check(p.reg('EIP') == 0x5a205b and p.uint(p.reg('ESP')) == 2,
          'start selects third slot and reaches binder call without a local two-input guard')
    check(p.uint(f.state(2)) == third and p.uint(f.state(2) + 0x48) == 0,
          'third candidate configured, no native evaluator insertion performed')
    check(f.snapshot_inputs() == before, 'safety stop leaves the two evaluator inputs unchanged')
    check(p.uint(f.teb) == 0xffffffff, 'start boundary has no live guest SEH frame')
    f.close()


def main():
    request_fields_and_restart()
    third_candidate_stopped_before_binding()
    print(f'PASS {checks}/{checks}: original actor start/priority and pre-binder safety checks')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
