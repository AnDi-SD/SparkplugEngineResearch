#!/usr/bin/env python3
"""Bounded original actor node discovery and SAN input binding evidence.

No host OS calls/game launch. Real object constructors, node discovery, name
registry, actor slot map, input insertion and normal teardown execute in guest.
Only stream/name-owner/char_traits/allocation/event/engine-storage seams remain.
"""
from pathlib import Path
import sys
from inspect_pc_san_keys import DEFAULT, inspect, u32
from pc_instruction_emulator import run_bounded
from pc_stl_fixtures import install_char_traits
from probe_pc_animation_manager import registry_entries
from probe_pc_san_reader import ReaderFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class ActorBindingFixture(ReaderFixture):
    def __init__(self,playback_capacity=None):
        raw = (DEFAULT / 'bbush.san').read_bytes()
        self.summary, self.tracks = inspect(DEFAULT / 'bbush.san')
        super().__init__(raw[u32(raw, 20) + 8:])
        p = self.p
        del p.seams[0x454370]
        del p.seams[0x453b10]
        install_char_traits(p)
        # Execute the original static initializer for matrix identity at7600BC.
        # Node constructor copies this global after first constructing identity;
        # a PE-only guest without CRT startup would otherwise overwrite it with0.
        self.call(0x6d38e0)
        p.put_uint(0x768e90, 0x5daf152d)  # explicit preinitialized RTTI identity fixture
        self.manager = self.call(0x454640)
        if playback_capacity is not None:
            if playback_capacity not in (0,2,19,40):raise ValueError('bounded observed actor capacities')
            p.put_uint(0x741654,playback_capacity)
        self.actor = self.call(0x5a3620)
        self.serializer = self.call(0x43dab0)
        self.animations = []
        self.nodes = []
        self.events = []
        self.engine = p.allocate(0x160)
        p.put_uint(0x755274, self.engine)
        p.seams[0x40fa10] = self.event

    def event(self, p):
        sp = p.reg('ESP')
        args = tuple(p.uint(sp + 4 * i) for i in range(1, 6))
        check(p.reg('ECX') == self.actor and args[1] == self.engine + 0x110,
              'binder event sender and engine queue receiver')
        check(args[2] == 0xffffffff, 'binder event broadcast sentinel')
        self.events.append(args)
        p.fixture_return(20)

    def load_animation(self):
        self.position = 0
        animation = self.call(0x41a090)
        result = self.call(0x43ecc0, this=self.serializer + 0x10, args=(self.stream, animation))
        check(result & 255 == 1 and not self.errors and self.position == len(self.data),
              'real bbush SAN reader with original registry')
        self.animations.append(animation)
        return animation

    def named_node(self, name):
        p = self.p
        node = self.call(0x421e20)
        raw = name.encode('latin1') + b'\0'
        text = p.allocate(len(raw))
        p.mu.mem_write(text, raw)
        self.call(0x4130f0, this=node, args=(text,))  # explicit named-object ownership seam
        self.nodes.append(node)
        return node

    def discover(self, node):
        self.call(0x5a33f0, this=self.actor, args=(node,))

    def controllers(self):
        p = self.p
        begin, end = p.uint(self.actor + 0x40), p.uint(self.actor + 0x44)
        if not begin:
            return []
        if (end - begin) % 4 or not 0 <= end - begin <= 64:
            raise AssertionError('bounded controller vector')
        return [p.uint(begin + offset) for offset in range(0, end - begin, 4)]

    def state(self, index):
        if not 0 <= index < self.p.uint(self.actor+0x2c):
            raise ValueError('default actor state bound')
        return self.p.uint(self.actor + 0x28) + index * 0x60

    def prepare(self, index, animation, *, priority=10, exclusive=True):
        p = self.p
        state = self.state(index)
        p.put_uint(state, animation)
        p.put_uint(state + 4, 1)
        p.put_floats(state + 0xc, [1])
        p.put_uint(state + 0x10, 0 if exclusive else 2)
        p.put_floats(state + 0x30, [1])
        p.mu.mem_write(state + 0x4c, b'\1')
        p.put_uint(state + 0x50, priority)
        return state

    def rebind(self, selected=0xffffffff):
        # Conservative host preflight: never even attempt a third distinct
        # state in the native two-input insertion routine. This is NOT a
        # discovered native invariant or capacity guard.
        eligible = [i for i in range(self.p.uint(self.actor+0x2c)) if self.p.uint(self.state(i) + 0x48) or i == selected]
        if len(eligible) > 2:
            raise ValueError('refusing potentially unsafe third native input')
        self.events.clear()
        self.call(0x5a1c10, this=self.actor, args=(selected,))

    def snapshot_inputs(self):
        p = self.p
        result = []
        for controller in self.controllers():
            evaluator = p.uint(controller + 0x14)
            result.append((p.uint(evaluator + 0x10), p.uint(evaluator + 0x14), [
                (p.uint(evaluator + 0x18 + i * 0x30), p.uint(evaluator + 0x1c + i * 0x30),
                 p.uint(evaluator + 0x20 + i * 0x30)) for i in range(2)]))
        return result

    def close(self):
        p = self.p
        self.call(0x5a35a0, this=self.actor, args=(1,))
        for node in self.nodes:
            if node not in self.freed:  # nodes not accepted by discovery retain external fixture ownership
                self.call(0x422220, this=node, args=(1,))
        for animation in self.animations:
            self.call(p.uint(p.uint(animation)), this=animation, args=(1,))
        self.call(p.uint(p.uint(self.serializer)), this=self.serializer, args=(1,))
        debug = p.uint(0x75526c)
        if debug:
            self.call(p.uint(p.uint(debug)), this=debug, args=(1,))
        check(registry_entries(p, self.manager) == {}, 'node/animation release empties original name registry')
        self.call(0x4545d0, this=self.manager, args=(1,))
        check(set(self.freed) == set(self.allocations), 'all tracked native binding objects released')


def discovery_and_single_binding():
    f = ActorBindingFixture()
    p = f.p
    animation = f.load_animation()
    for track in f.tracks[:2]:
        node = f.named_node(track['name'])
        check(p.uint(node + 0xb0) == 0x70a00, 'executed native PC node default flags')
        f.discover(node)
        check(p.uint(node + 0xb0) == 0x72a00, 'actor marks node controller ownership bit2000')
    check(len(f.controllers()) == 2, 'one real node controller per selected node')
    f.discover(f.nodes[0])
    check(len(f.controllers()) == 2, 'already marked node is not added twice')
    state = f.prepare(0, animation)
    f.rebind(0)
    values = f.snapshot_inputs()
    for index, (slot, count, inputs) in enumerate(values):
        track = p.uint(animation + 0x1c) + index * 0x44
        check(slot == p.uint(track + 0x14) and count == 1 and inputs[0] == (state, track, 10),
              'real actor map resolves SAN slot into matching evaluator input')
    check(p.uint(state + 0x48) == 2, 'selected state receives one binding-use increment per matching controller')
    check([event[0] for event in f.events] == [4], 'newly used animation sends one gained-binding event')
    f.rebind()
    check(p.uint(state + 0x48) == 4 and not f.events,
          'repeated exclusive rebind increments original counter again; not a simple live-input refcount')
    p.put_uint(state + 0x48, 0)
    f.rebind()
    check(all(count == 1 and all(pair[:2] == (0, 0) for pair in inputs)
              for _, count, inputs in f.snapshot_inputs()),
          'zero-use state cleanup clears pointers but keeps evaluator count/priority')
    f.close()


def two_blended_inputs():
    f = ActorBindingFixture()
    p = f.p
    animations = [f.load_animation() for _ in range(2)]
    for track in f.tracks[:2]:
        f.discover(f.named_node(track['name']))
    first = f.prepare(0, animations[0], priority=10, exclusive=False)
    second = f.prepare(1, animations[1], priority=5, exclusive=False)
    f.rebind(0)
    f.rebind(1)
    check([p.uint(state + 0x48) for state in (first, second)] == [2, 2],
          'two nonexclusive states count matching bindings')
    check([event[0] for event in f.events] == [4] and f.events[0][3] == second,
          'only newly selected state emits gained event')
    for _, count, inputs in f.snapshot_inputs():
        check(count == 2 and [entry[0] for entry in inputs] == [second, first] and
              [entry[2] for entry in inputs] == [5, 10], 'native inputs sorted by unsigned priority')
    f.rebind()
    check([p.uint(state + 0x48) for state in (first, second)] == [2, 2] and not f.events,
          'nonexclusive duplicate rebind has stable use counts')
    # A higher-priority exclusive input discards second physical slot only;
    # first-slot replacement does not have a symmetric counter decrement.
    p.put_uint(second + 0x10, 0)
    p.put_uint(second + 0x50, 20)
    f.rebind()
    check([p.uint(state + 0x48) for state in (first, second)] == [0, 4],
          'exclusive replacement decrements displaced second slot, increments incoming state')
    check([event[0] for event in f.events] == [5] and f.events[0][3] == first,
          'state losing all counted bindings emits lost event')
    check(all(count == 1 and inputs[0][0] == second and inputs[1][:2] == (0, 0)
              for _, count, inputs in f.snapshot_inputs()), 'exclusive path leaves only one input')
    f.close()


def duplicate_node_names():
    f = ActorBindingFixture()
    p = f.p
    animation = f.load_animation()
    name = f.tracks[0]['name']
    f.discover(f.named_node(name))
    f.discover(f.named_node(name))
    controllers = f.controllers()
    check(len(controllers) == 2, 'distinct nodes with same name both get controllers')
    check(p.uint(f.actor + 0x38) == 1, 'actor slot map has one entry for duplicate names')
    state = f.prepare(0, animation)
    f.rebind(0)
    inputs = f.snapshot_inputs()
    check(inputs[0][0] == inputs[1][0] and inputs[0][1] == 0 and inputs[1][1] == 1,
          'last same-name node overwrites map value; earlier evaluator stays unbound')
    check(p.uint(state + 0x48) == 1, 'duplicate name produces only one matched evaluator input')
    f.close()


def main():
    discovery_and_single_binding()
    two_blended_inputs()
    duplicate_node_names()
    print(f'PASS {checks}/{checks}: original actor discovery/input-binding checks')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
