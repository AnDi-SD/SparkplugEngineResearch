#!/usr/bin/env python3
"""Bounded native/portable real-SAN actor start/tick/input/node/stop sequences.

Native manager really dispatches actor. Portable observation fixture advances an
empty-gated manager then calls the same actor tick explicitly to collect actions;
the direct portable manager->actor path has separate C++ integration checks.
Each snapshot explicitly forces node world update, not an inferred frame hook.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from pc_instruction_emulator import ROOT, run_bounded
from inspect_pc_san_keys import DEFAULT
from probe_pc_actor_tree_stop import StopFixture
from probe_pc_actor_tick import ActorFixture, bits
from compare_pc_san_reader import compare

SCENARIOS = ('normal', 'blend', 'oneshot', 'transition', 'fade_stop', 'suppressed')


class ScenarioFixture(StopFixture):
    def __init__(self):
        super().__init__()
        self.resources = [self.animation, self.load_animation(), self.load_animation()]
        p = self.p
        p.mu.mem_map(0x34030000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        p.put_uint(p.uint(0x5a241c + 2), 0x34030010)
        p.seams[0x34030010] = lambda p: ActorFixture.floor(None, p)
        p.seams[0x60dd44] = lambda p: ActorFixture.fmod(None, p)

    def capture(self):
        p = self.p
        states = []
        for i in range(3):
            s = self.state(i)
            u = lambda offset: p.uint(s + offset)
            f = lambda offset: p.floats(s + offset, 1)[0]
            animation = u(0)
            states.append([self.resources.index(animation) if animation in self.resources else -1,
                           u(4), u(8) & 255, f(0xc), u(0x10), f(0x18), f(0x20), f(0x24),
                           int(u(0x28) != 0), u(0x2c), f(0x30), f(0x34), u(0x3c) & 255,
                           u(0x40), u(0x44), u(0x48), u(0x4c) & 255, u(0x50), f(0x54), f(0x58), f(0x5c)])
        inputs = []
        for controller in self.controllers():
            e = p.uint(controller + 0x14)
            value = [p.uint(e + 0x10), p.uint(e + 0x14)]
            for i in range(2):
                address = e + 0x18 + i * 0x30
                state = p.uint(address)
                index = next((j for j in range(40) if state == self.state(j)), -1)
                value.append([index, int(p.uint(address + 4) != 0), p.uint(address + 8),
                              *[p.uint(address + 0xc + 4 * j) for j in range(9)]])
            inputs.append(value)
        nodes = []
        for node in self.nodes:
            self.call(0x421420, this=node, args=(1,))  # explicit fixture endpoint, not engine frame evidence
            nodes.append([*p.floats(node + 0x20, 3), *p.floats(node + 0x30, 3),
                          *p.floats(node + 0x40, 9), *p.floats(node + 0x74, 3),
                          *p.floats(node + 0x80, 3), *p.floats(node + 0x8c, 9), p.uint(node + 0xb0)])
        events = []
        for action in self.order:
            if action[0] == 'flush':
                events.append([2])
                continue
            if action[0] == 'queued':
                _, code, queue, delay, state, payload = action
                kind = 0
            else:
                _, code, state, payload = action
                kind = 1
            index = next((i for i in range(40) if state == self.state(i)), -1)
            payload_kind = 1 if code in (4, 5) else 2 if code == 11 else 0
            events.append([kind, code, index, payload_kind, p.uint(payload + 0x18) if code == 11 else -1])
        return {'frame': p.uint(self.manager + 0x10), 'states': states, 'inputs': inputs,
                'nodes': nodes, 'events': events}

    def sequence(self, name):
        p = self.p
        results = []

        def start(index, fade, mode=1, transition=0, out_duration=0):
            self.order.clear()
            self.start(animation=self.resources[index], fade=fade, mode=mode,
                       transition=transition, fade_out_duration=out_duration)
            results.append(self.capture())

        def tick(delta):
            self.order.clear()
            p.put_uint(self.engine + 0xb8, bits(delta))
            self.call(0x4535a0, this=self.manager)
            results.append(self.capture())

        def stop(index, suppress):
            self.stop(self.resources[index], suppress=suppress)
            results.append(self.capture())

        if name == 'normal':
            start(0, 0); tick(.25); tick(.75); tick(.125); stop(0, False)
        elif name == 'blend':
            start(0, 2); tick(.25); start(1, 2); tick(.25); start(1, 0); tick(.25)
            self.order.clear()
            self.call(0x5a21c0, this=self.actor)
            results.append(self.capture())
        elif name == 'oneshot':
            start(0, 0, 0); tick(.75); tick(.5); tick(.25)
        elif name == 'transition':
            start(0, 0, 1, .5); tick(.25); tick(.25); tick(.25)
        elif name == 'fade_stop':
            start(0, 3, 1, 0, .5)
            p.mu.mem_write(self.state(0) + 0x3c, b'\1')
            tick(.75); tick(.25)
        elif name == 'suppressed':
            start(0, 2); start(1, 2); stop(0, True); tick(.25); stop(1, True)
        else:
            raise ValueError('Unknown bounded scenario')
        return results


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', type=Path, required=True)
    parser.add_argument('--scenario', choices=SCENARIOS, default='normal')
    args = parser.parse_args(argv)
    portable = args.portable.resolve()
    if not portable.is_relative_to(ROOT) or portable.name != 'SparkplugActorBindingTests.exe':
        raise ValueError('Only bounded workspace test binary allowed')
    output = subprocess.run([str(portable), '--scenario', str(DEFAULT / 'bbush.san'), args.scenario],
                            cwd=ROOT, capture_output=True, text=True, timeout=10, check=True).stdout
    if len(output) > 150000:
        raise ValueError('Actor scenario output bound')
    actual = json.loads(output)
    f = ScenarioFixture()
    expected = f.sequence(args.scenario)
    f.close()
    count = compare(expected, actual)
    print(f'PASS {count}/{count}: {args.scenario} full original/portable SAN actor/input/node sequence')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
