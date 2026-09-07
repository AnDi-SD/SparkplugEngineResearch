#!/usr/bin/env python3
"""Bounded original actor descendant wrapper and Stop/StopAll evidence.

Child edges are explicit synthetic list fixtures; constructors/discovery/walker,
Start/Stop/binder/registry and normal object teardown execute original code.
Fixtures restore empty lists before teardown, so native Attach is not claimed.
Event queues/immediate dispatch remain recorded seams, never host callbacks.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_actor_binding import ActorBindingFixture
from probe_pc_actor_start import StartFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def descendant_wrapper():
    f = ActorBindingFixture()
    p = f.p
    animation = f.load_animation()
    root = f.named_node(f.tracks[0]['name'])
    first = f.named_node(f.tracks[0]['name'])
    grand = f.named_node(f.tracks[1]['name'])
    second = f.named_node(f.tracks[2]['name'])
    inactive = f.named_node(f.tracks[3]['name'])
    last = f.named_node(f.tracks[4]['name'])
    p.put_uint(inactive + 0xb0, p.uint(inactive + 0xb0) & ~0x800)
    edges = ((root, (first, second, inactive)), (first, (grand,)), (inactive, (last,)))
    saved = []
    for parent, children in edges:
        head = p.uint(parent + 0x18)
        saved.append((head, bytes(p.mu.mem_read(head, 12)), parent, p.uint(parent + 0x1c)))
        links = [p.allocate(12) for _ in children]
        p.put_uint(head, links[0])
        p.put_uint(head + 4, links[-1])
        p.put_uint(parent + 0x1c, len(children))
        for index, (link, child) in enumerate(zip(links, children)):
            p.put_uint(link, links[index + 1] if index + 1 < len(links) else head)
            p.put_uint(link + 4, links[index - 1] if index else head)
            p.put_uint(link + 8, child)
    f.call(0x5a35c0, this=f.actor, args=(root,))
    check([p.uint(c + 0x10) for c in f.controllers()] == [first, grand, second, last],
          'wrapper skips root, preorder includes descendants of ineligible parent')
    check(not p.uint(root + 0xb0) & 0x2000 and not p.uint(inactive + 0xb0) & 0x2000,
          'excluded root and nonanimated intermediary are not marked')
    check(all(p.uint(n + 0xb0) & 0x2000 for n in (first, grand, second, last)),
          'accepted descendants marked by original discovery')
    f.call(0x5a35c0, this=f.actor, args=(root,))
    check(len(f.controllers()) == 4, 'second tree bind does not duplicate marked descendants')
    f.prepare(0, animation)
    f.rebind(0)
    check(p.uint(f.state(0) + 0x48) == 4, 'real SAN inputs reach four descendant evaluators')
    for head, data, parent, count in saved:
        p.mu.mem_write(head, data)
        p.put_uint(parent + 0x1c, count)
    f.close()


class StopFixture(StartFixture):
    def __init__(self):
        self.order = []
        super().__init__()
        self.p.seams[0x40f9a0] = self.immediate

    def event(self, p):
        super().event(p)
        self.order.append(('queued', *self.events[-1]))

    def flush(self, p):
        super().flush(p)
        self.order.append(('flush',))

    def immediate(self, p):
        args = tuple(p.uint(p.reg('ESP') + 4 * i) for i in range(1, 4))
        check(p.reg('ECX') == self.actor, 'immediate stopped-event sender')
        self.order.append(('immediate', *args))
        p.fixture_return(12)

    def stop(self, animation, suppress=False):
        self.order.clear()
        self.events.clear()
        self.flushes = 0
        self.call(0x5a20a0, this=self.actor, args=(animation, int(suppress)))


def stop_and_stop_all():
    f = StopFixture()
    p = f.p
    second = f.load_animation()
    missing = f.load_animation()
    f.start(fade=2)
    f.stop(missing)
    check(not f.order and p.uint(f.state(0) + 0x48) == 2, 'unmatched Stop is a complete no-op')
    f.stop(f.animation, suppress=True)
    check(f.order == [('flush',)], 'suppressed Stop flushes without explicit stopped event')
    check(p.uint(f.state(0) + 0x48) == 0 and p.uint(f.state(0) + 0x4c) & 255 == 0 and
          p.uint(f.state(0) + 0x40) == 3, 'Stop resets uses/running and sets status3')
    check(all(count == 1 and all(pair[:2] == (0, 0) for pair in slots)
              for _, count, slots in f.snapshot_inputs()), 'Stop clears physical pointers but keeps count')
    f.stop(f.animation)
    check(f.order == [('flush',), ('immediate', 3, f.state(0), f.animation)],
          'inactive matching animation still flushes then immediate event3 state/animation')
    f.start(fade=2)
    f.start(animation=second, fade=2)
    f.stop(f.animation)
    check(p.uint(f.state(1) + 0x48) == 2, 'Stop rebind keeps remaining blended-state counter')
    check(all(count == 1 and slots[0][0] == f.state(1) for _, count, slots in f.snapshot_inputs()),
          'remaining state promoted to physical slot0')
    check(f.order == [('flush',), ('immediate', 3, f.state(0), f.animation)],
          'no extra gained/lost events for remaining state')
    f.order.clear()
    f.call(0x5a21c0, this=f.actor)
    check(f.order == [('flush',), ('immediate', 3, f.state(1), second)],
          'StopAll calls non-suppressed Stop for used state only')
    check(all(p.uint(f.state(i) + 0x48) == 0 for i in range(40)), 'StopAll clears both states')
    f.order.clear()
    f.call(0x5a21c0, this=f.actor)
    check(not f.order, 'second StopAll does not revisit inactive states')
    f.close()


def main():
    descendant_wrapper()
    stop_and_stop_all()
    print(f'PASS {checks}/{checks}: original descendant wrapper and Stop/StopAll checks')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
