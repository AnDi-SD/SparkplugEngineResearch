#!/usr/bin/env python3
"""Bounded original PC animation-manager frame/list evidence; no OS or game launch."""
from pathlib import Path
import sys
from pc_instruction_emulator import PcInstructions, run_bounded
from probe_pc_actor_tick import ActorFixture
from probe_pc_animation_lifecycle import LifetimeFixture
from pc_stl_fixtures import install_char_traits, read_cstring

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class ManagerFixture(LifetimeFixture):
    """Original constructor and map with allocator/char_traits boundaries only."""
    def __init__(self):
        super().__init__()
        install_char_traits(self.p)
        self.manager = self.call(0x454640)
        self.name_pointers = {}

    def name(self, text):
        raw = text.encode('latin1') if isinstance(text, str) else text
        if b'\0' in raw or len(raw) > 255:
            raise ValueError('bounded name fixture requires at most 255 non-NUL bytes')
        if raw not in self.name_pointers:
            address = self.p.allocate(len(raw) + 1)
            self.p.mu.mem_write(address, raw + b'\0')
            self.name_pointers[raw] = address
        return self.name_pointers[raw]

    def bind(self, text):
        return self.call(0x454370, this=self.manager, args=(self.name(text),))

    def unbind(self, text):
        self.call(0x453b10, this=self.manager, args=(self.name(text),))

    def entries(self):
        return registry_entries(self.p, self.manager)

    def close(self):
        self.call(0x4545d0, this=self.manager, args=(1,))


def registry_entries(p, manager):
    """Read-only verified MSVC tree observation, not a replacement map search."""
    sentinel = p.uint(manager + 0x1c)
    pending, seen, result = [p.uint(sentinel + 4)], set(), {}
    while pending:
        node = pending.pop()
        if node == sentinel:
            continue
        if node in seen or len(seen) >= 256:
            raise AssertionError('name map cycle/fixture traversal bound')
        seen.add(node)
        size, capacity = p.uint(node + 0x20), p.uint(node + 0x24)
        check(size <= capacity and size <= 255, 'native map string bounds')
        text = read_cstring(p, p.uint(node + 0x10) if capacity >= 16 else node + 0x10, 256)
        check(len(text) == size and text not in result, 'native map distinct string key')
        result[text] = (p.uint(node + 0x28), p.uint(node + 0x2c))
        pending.extend((p.uint(node), p.uint(node + 8)))
    check(len(result) == p.uint(manager + 0x20), 'tree cardinality matches manager entry count')
    return result


def registry_lifetime():
    f = ManagerFixture()
    p, manager = f.p, f.manager
    check(f.allocations[manager] == 0x2c, 'native manager extent')
    check(p.uint(manager) == 0x6e6e30 and p.uint(0x75f880) == manager, 'manager vtable/global')
    check(p.uint(manager + 0x10) == p.uint(manager + 0x14) == 1, 'frame and next-slot defaults are one')
    check(p.uint(manager + 0x20) == p.uint(manager + 0x24) == p.uint(manager + 0x28) == 0,
          'empty registry and controller list defaults')
    check(f.allocations[p.uint(manager + 0x1c)] == 0x34, 'native name tree sentinel extent')
    expected = {}
    texts = ['Head', 'Head', 'head', 'Back', 'HeadX', '', 'A'*15, 'A'*16, 'A'*80, '\x80Node']
    next_slot = 1
    for text in texts:
        raw = text.encode('latin1')
        old = expected.get(raw)
        slot = old[0] if old else next_slot
        if not old:
            next_slot += 1
        expected[raw] = (slot, old[1] + 1 if old else 1)
        check(f.bind(text) == slot, 'case-sensitive monotonic IDs and duplicate binding')
        check(f.entries() == expected, 'all keys and reference counts after bind')
    check(p.uint(manager + 0x14) == next_slot, 'duplicates do not consume slot IDs')
    f.unbind('not-bound')
    check(f.entries() == expected, 'unbinding missing name is a no-op')
    for text in texts:
        raw = text.encode('latin1')
        slot, refs = expected[raw]
        if refs == 1:
            del expected[raw]
        else:
            expected[raw] = (slot, refs - 1)
        f.unbind(text)
        check(f.entries() == expected, 'unbind releases reference or removes last-reference node')
    check(f.bind('Head') == next_slot, 'removed ID is not reused')
    check(f.bind('A'*80) == next_slot + 1, 'long name can be rebound after heap string release')
    f.close()  # nonempty map destruction, without explicit unbind of last two names
    check(p.uint(0x75f880) == 0, 'destructor clears global manager')
    check(set(f.freed) == set(f.allocations), 'manager, map nodes, long-string allocations all released')


def subcontroller_binding():
    f = ManagerFixture()
    p = f.p
    controller, vt = p.allocate(0x18), p.allocate(0x30)
    evaluator = f.call(0x5ff090)  # verified spTransformTrackEval factory
    p.put_uint(controller, vt)
    p.mu.mem_map(0x34020000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
    p.put_uint(vt + 0x24, 0x34020010)
    p.put_uint(vt + 0x2c, 0x34020020)
    calls = []

    def get_eval(machine):
        calls.append('evaluator')
        machine.fixture_return(eax=evaluator)

    def get_name(machine):
        calls.append('name')
        machine.fixture_return(eax=f.name('Head'))

    p.seams[0x34020010] = get_eval
    p.seams[0x34020020] = get_name
    # The PE loader/global initializers are deliberately not run. Start with
    # absent RTTI, then supply only the proven identity word normally written
    # by the registration block ending at call 0x6D79C0. Original IsKindOf runs.
    result = f.call(0x4545f0, this=f.manager, args=(controller,))
    check(result == 0 and p.uint(evaluator + 0x10) == 0xffffffff,
          'unrecognized evaluator is rejected without slot change')
    check(calls == ['evaluator'] and f.entries() == {}, 'rejection does not obtain name or bind it')
    p.put_uint(0x768e90, 0x5daf152d)
    calls.clear()
    result = f.call(0x4545f0, this=f.manager, args=(controller,))
    check(result == 1 and p.uint(evaluator + 0x10) == 1, 'attach returns and stores matching track-evaluator slot')
    check(calls == ['evaluator', 'evaluator', 'name'], 'attach checks RTTI before second getter/name')
    check(f.entries() == {b'Head': (1, 1)}, 'attach contributes one name reference')
    f.call(0x453e90, this=f.manager, args=(controller,))
    check(f.entries() == {} and p.uint(evaluator + 0x10) == 1, 'detach unbinds name but does not reset evaluator slot')
    f.call(p.uint(p.uint(evaluator)), this=evaluator, args=(1,))
    f.close()
    check(set(f.freed) == set(f.allocations), 'controller-binding fixture native allocations released')


def blank_clone():
    f = ManagerFixture()
    p = f.p
    f.bind('Head')
    p.put_uint(f.manager + 0x10, 99)
    registry = p.allocate(0x20)
    p.put_uint(0x74e060, registry)
    mappings = []

    def register_clone(machine):
        check(machine.reg('ECX') == registry, 'clone registry fixture receiver')
        sp = machine.reg('ESP')
        mappings.append((machine.uint(sp + 4), machine.uint(sp + 8)))
        machine.fixture_return(8)

    p.seams[0x412f70] = register_clone
    clone = f.call(0x4546a0, this=f.manager)
    check(clone != f.manager and mappings == [(f.manager, clone)], 'original clone registers source/result')
    check(p.uint(clone + 0x10) == p.uint(clone + 0x14) == 1, 'clone has fresh counters')
    check(registry_entries(p, clone) == {} and p.uint(clone + 0x24) == p.uint(clone + 0x28) == 0,
          'root copy does not copy name registry/controller list')
    check(p.uint(0x75f880) == clone, 'clone constructor replaces singleton')
    f.close()
    check(p.uint(0x75f880) == 0, 'original destructor clears singleton unconditionally, even if not current')
    f.call(0x4545d0, this=clone, args=(1,))
    check(set(f.freed) == set(f.allocations), 'both manager instances and owned maps released')


def controller_copy_and_actor_clone():
    f = ManagerFixture()
    p = f.p
    source, target = [f.call(0x5a3620) for _ in range(2)]
    registry = p.allocate(0x20)
    p.put_uint(0x74e060, registry)
    mappings = []

    def register_clone(machine):
        sp = machine.reg('ESP')
        mappings.append((machine.uint(sp + 4), machine.uint(sp + 8)))
        machine.fixture_return(8)

    p.seams[0x412f70] = register_clone
    p.mu.mem_write(source + 0x10, b'\0')
    p.mu.mem_write(source + 0x1c, b'\0')
    p.mu.mem_write(source + 0x24, b'\0')
    p.put_floats(source + 0x20, [7])
    p.put_floats(p.uint(source + 0x28) + 0x34, [12])
    before = bytes(p.mu.mem_read(target, 0x54))
    result = f.call(0x423100, this=source, args=(target,))
    after = bytes(p.mu.mem_read(target, 0x54))
    check(result & 255 == 1, 'original protected controller copy succeeds')
    check([index for index, pair in enumerate(zip(before, after)) if pair[0] != pair[1]] == [0x10],
          'controller copy changes only enabled byte, not links or actor payload')
    clone = f.call(0x5a3680, this=source)
    check(mappings == [(source, clone)], 'original actor clone registers source/result')
    check(bytes(p.mu.mem_read(clone + 0x10, 1)) == b'\0', 'actor clone inherits controller enabled flag')
    check(p.floats(clone + 0x20, 1)[0] == 1 and
          bytes(p.mu.mem_read(clone + 0x1c, 1)) == bytes(p.mu.mem_read(clone + 0x24, 1)) == b'\1',
          'actor-specific flags and speed remain constructor defaults')
    check(p.floats(p.uint(clone + 0x28) + 0x34, 1)[0] == 0 and p.uint(clone + 0x2c) == 40,
          'actor clone gets fresh empty playback states')
    check(p.uint(f.manager + 0x24) == source and p.uint(source + 0x14) == target and
          p.uint(target + 0x14) == clone and p.uint(f.manager + 0x28) == clone,
          'cloned actor is registered at tail, source links are not copied')
    for actor in (target, clone, source):
        f.call(0x5a35a0, this=actor, args=(1,))
    f.close()
    check(set(f.freed) == set(f.allocations), 'source/target/clone normal destruction frees native storage')


def frame_to_actor():
    fixture = ActorFixture()
    p = fixture.p
    manager = p.allocate(0x60)  # observed manager storage fixture, not native constructor
    for enabled, applies, advances, expected_time, expected_actions in (
        (True, True, True, 1, [['get-evaluator'], ['direct', 1.0], ['flush']]),
        (False, True, True, 0, []),
        (True, False, True, 1, [['flush']]),
        (True, False, False, 0, []),
    ):
        fixture.prepare(applies=applies, advance=advances)
        p.put_uint(fixture.actor, 0x703f80)
        p.mu.mem_write(fixture.actor + 0x10, bytes([enabled]))
        p.put_uint(manager + 0x24, fixture.actor)
        p.put_uint(manager + 0x28, fixture.actor)
        p.put_floats(fixture.engine + 0xb8, [1])
        previous = p.uint(manager + 0x10)
        p.run(0x4535a0, this=manager)
        check(p.uint(manager + 0x10) == previous + 1, 'manager frame counter increments before gate')
        check(p.floats(fixture.state + 0x34, 1)[0] == expected_time, 'manager-to-real-actor sample time')
        check(fixture.events == expected_actions, 'manager/actor/controller gate and action order')
    p.put_uint(manager + 0x24, 0)
    p.put_uint(manager + 0x28, 0)
    p.put_uint(manager + 0x10, 0xffffffff)
    p.run(0x4535a0, this=manager)
    check(p.uint(manager + 0x10) == 0, 'counter wraps even for an empty controller list')


def frame_snapshot_and_order():
    p = PcInstructions()
    manager, engine = p.allocate(0x60), p.allocate(0x100)
    p.put_uint(0x755274, engine)
    p.put_floats(engine + 0xb8, [.25])
    objects = [p.allocate(0x1c) for _ in range(3)]
    vt = p.allocate(0x20)
    p.mu.mem_map(0x34000000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
    p.put_uint(vt + 0x1c, 0x34000010)
    called = []

    def update(machine):
        called.append((machine.reg('ECX'), machine.floats(machine.reg('ESP') + 4, 1)[0]))
        p.put_floats(engine + 0xb8, [9])  # test mutation after first callback
        machine.fixture_return(4)

    p.seams[0x34000010] = update
    for index, obj in enumerate(objects):
        p.put_uint(obj, vt)
        p.mu.mem_write(obj + 0x10, bytes([index != 1]))
        p.put_uint(obj + 0x14, objects[index + 1] if index + 1 < len(objects) else 0)
    p.put_uint(manager + 0x24, objects[0])
    p.put_uint(manager + 0x28, objects[-1])
    p.run(0x4535a0, this=manager)
    check(called == [(objects[0], .25), (objects[2], .25)], 'head-to-next order, disabled skip, one delta snapshot')
    check(p.floats(engine + 0xb8, 1)[0] == 9, 'callback fixture really changed engine delta')


def controller_list_lifetime():
    fixture = LifetimeFixture()
    p = fixture.p
    manager = p.allocate(0x60)
    p.put_uint(0x75f880, manager)
    actors = [fixture.call(0x5a3620) for _ in range(3)]

    def verify(expected):
        check(p.uint(manager + 0x24) == (expected[0] if expected else 0), 'original controller list head')
        check(p.uint(manager + 0x28) == (expected[-1] if expected else 0), 'original controller list tail')
        for index, obj in enumerate(expected):
            check(p.uint(obj + 0x14) == (expected[index + 1] if index + 1 < len(expected) else 0), 'original next link')
            check(p.uint(obj + 0x18) == (expected[index - 1] if index else 0), 'original previous link')

    verify(actors)
    fixture.call(0x5a35a0, this=actors[1], args=(1,))
    verify([actors[0], actors[2]])
    fixture.call(0x5a35a0, this=actors[0], args=(1,))
    verify([actors[2]])
    fixture.call(0x5a35a0, this=actors[2], args=(1,))
    verify([])
    check(set(fixture.freed) == set(fixture.allocations), 'all three actors and their arrays/sentinels released')


def main():
    registry_lifetime()
    subcontroller_binding()
    blank_clone()
    controller_copy_and_actor_clone()
    frame_to_actor()
    frame_snapshot_and_order()
    controller_list_lifetime()
    print(f'PASS {checks}/{checks}: original animation-manager registry/frame/list checks')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
