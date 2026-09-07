#!/usr/bin/env python3
"""Exact two-input insertion/cache/count observations in bounded PC guest.

Synthetic playback/track storage only; original evaluator factory and insertion
execute. Never calls nonexclusive insertion with a potential third live input.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class InputFixture(LifetimeFixture):
    def __init__(self):
        super().__init__()
        self.evaluator = self.call(0x5ff090)
        self.states = [self.p.allocate(0x60) for _ in range(3)]
        self.tracks = [self.p.allocate(0x44) for _ in range(3)]

    def prepare(self, slots, uses):
        p = self.p
        p.put_uint(self.evaluator + 0x14, len(slots))
        for index in range(2):
            state, track, priority, cache = slots[index] if index < len(slots) else (-1, -1, 0xffffffff, 200)
            address = self.evaluator + 0x18 + index * 0x30
            p.put_uint(address, self.states[state] if state >= 0 else 0)
            p.put_uint(address + 4, self.tracks[track] if track >= 0 else 0)
            p.put_uint(address + 8, priority)
            for key in range(9):
                p.put_uint(address + 12 + key * 4, cache + key)
        for state, count in zip(self.states, uses):
            p.put_uint(state + 0x48, count)

    def insert(self, state, track, priority, exclusive):
        p = self.p
        count = p.uint(self.evaluator + 0x14)
        if count > 2:
            raise ValueError('invalid existing input count')
        retained = [p.uint(self.evaluator + 0x18 + index * 0x30) for index in range(count)]
        retained = [pointer for pointer in retained if pointer and pointer != self.states[state]]
        if not exclusive and len(retained) + 1 > 2:
            raise ValueError('refusing unsafe third native blend input')
        self.call(0x5fe9c0, this=self.evaluator,
                  args=(self.states[state], self.tracks[track], priority, int(exclusive)))

    def snapshot(self):
        p = self.p
        result = {'count': p.uint(self.evaluator + 0x14), 'slots': [],
                  'uses': [p.uint(state + 0x48) for state in self.states]}
        for index in range(2):
            address = self.evaluator + 0x18 + index * 0x30
            state, track = p.uint(address), p.uint(address + 4)
            result['slots'].append([self.states.index(state) if state else -1,
                                    self.tracks.index(track) if track else -1,
                                    p.uint(address + 8),
                                    [p.uint(address + 12 + key * 4) for key in range(9)]])
        return result


def main():
    f = InputFixture()
    both = [(0, 0, 10, 100), (1, 1, 20, 200)]
    f.prepare(both, [1, 1, 0])
    before = f.snapshot()
    f.insert(2, 2, 19, True)
    check(f.snapshot() == before, 'exclusive insertion below any live priority is a complete no-op')
    f.insert(2, 2, 20, True)
    result = f.snapshot()
    check(result['count'] == 1 and result['uses'] == [1, 0, 1],
          'exclusive increments incoming and decrements old second slot, not old first slot')
    check(result['slots'][0][:3] == [2, 2, 20] and result['slots'][1][:3] == [-1, -1, 20],
          'exclusive clears only second pointers; priority persists')
    check([slot[3] for slot in result['slots']] == [list(range(100, 109)), list(range(200, 209))],
          'exclusive replacement does not reset either physical key cache')
    f.prepare([(0, 0, 10, 100)], [1, 0, 0])
    f.insert(1, 1, 5, False)
    result = f.snapshot()
    check(result['count'] == 2 and result['uses'] == [1, 1, 0] and
          [slot[:3] for slot in result['slots']] == [[1, 1, 5], [0, 0, 10]],
          'nonexclusive inserts lower-priority state first')
    check([slot[3] for slot in result['slots']] == [list(range(100, 109))] * 2,
          'new input retains destination cache; moved old input copies its own cache')
    f.prepare(both, [1, 1, 0])
    f.insert(0, 2, 30, False)
    result = f.snapshot()
    check(result['uses'] == [1, 1, 0] and [slot[:3] for slot in result['slots']] == [[1, 1, 20], [0, 2, 30]],
          'existing playback is removed/reinserted and track pointer can change')
    check([slot[3] for slot in result['slots']] == [list(range(200, 209))] * 2,
          'reprioritized incoming playback does not carry its former cache')
    f.prepare(both, [1, 1, 0])
    f.insert(0, 0, 20, False)
    check([slot[:3] for slot in f.snapshot()['slots']] == [[1, 1, 20], [0, 0, 20]],
          'equal priority insertion is after retained equal-priority input')
    f.prepare([(0, 0, 0xffffffff, 100)], [1, 0, 0])
    f.insert(1, 1, 0x80000000, False)
    check([slot[2] for slot in f.snapshot()['slots']] == [0x80000000, 0xffffffff],
          'priority ordering is unsigned')
    f.prepare(both, [1, 1, 0])
    f.call(0x5feb70, this=f.evaluator, args=(0,))
    result = f.snapshot()
    check(result['count'] == 2 and result['uses'] == [1, 1, 0] and
          result['slots'][0] == [-1, -1, 10, list(range(100, 109))],
          'clear changes pointers only, neither count/priority/cache nor playback counter')
    f.insert(2, 2, 5, False)
    check(f.snapshot()['count'] == 2 and f.snapshot()['uses'] == [1, 1, 1],
          'pointer-hole is compacted without decrementing its former playback counter')
    f.prepare(both, [1, 1, 0])
    try:
        f.insert(2, 2, 30, False)
        raise AssertionError('unsafe third input unexpectedly attempted')
    except ValueError:
        check(f.snapshot() == before, 'host third-input safety fence executes before native mutation')
    f.call(0x5ff070, this=f.evaluator, args=(1,))
    check(set(f.freed) == set(f.allocations), 'normal native evaluator destruction releases allocation')
    print(f'PASS {checks}/{checks}: original two-input insertion/cache checks')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
