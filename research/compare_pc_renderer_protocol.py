#!/usr/bin/env python3
"""PC original/portable callback-state and alpha-key differential checks.

Finite affine matrices and bounded callback vectors only. Native pre/post
wrappers execute; platform/user callbacks remain explicit recording seams.
"""
from pathlib import Path
import math
import random
import subprocess
import sys
from pc_instruction_emulator import ROOT, run_bounded
from probe_pc_renderable_callbacks import ProtocolFixture
from probe_pc_renderer_queues import QueueFixture

EXE = ROOT / '.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugRendererProtocolTests.exe'


def host(mode, rows):
    result = subprocess.run([str(EXE), mode], input='\n'.join(rows) + '\n',
                            capture_output=True, text=True, timeout=20, check=True)
    return [line.split(',') for line in result.stdout.splitlines()]


def keys():
    f = QueueFixture()
    p = f.p
    node, model, mesh = f.model_with_node()
    rng = random.Random(0x454c30)
    rows, expected = [], []

    def matrix():
        angle = rng.uniform(-math.pi, math.pi)
        c, s = math.cos(angle), math.sin(angle)
        scales = [rng.choice((-3., -.5, .25, 1., 2.)) for _ in range(3)]
        return (c * scales[0], s * scales[0], 0., 0.,
                -s * scales[1], c * scales[1], 0., 0.,
                0., 0., scales[2], 0.,
                *[rng.uniform(-20, 20) for _ in range(3)], 1.)

    for index in range(64):
        center = [rng.uniform(-8, 8) for _ in range(3)]
        world, view = matrix(), matrix()
        p.put_floats(mesh + 0x18, (*center, rng.uniform(0, 10)))
        p.put_floats(node + 0x138, world)
        p.put_floats(f.camera + 0xcc, view)
        # Feed portable binary exactly the same float32 fixture inputs.
        center, world, view = p.floats(mesh + 0x18, 3), p.floats(node + 0x138, 16), p.floats(f.camera + 0xcc, 16)
        branch, priority, base = index % 2, rng.randrange(0x100000000), rng.randrange(0x100000000)
        p.mu.mem_write(f.camera + 0x231, bytes([branch]))
        p.put_uint(f.renderer + 0x48, base)
        p.put_uint(f.renderer + 0x4c, 0)
        assert f.enqueue(node, model, priority) == 1
        record = f.record(0)
        expected.append(record[3:])
        rows.append(' '.join(map(str, (*center, *world, *view, branch, priority, base, 0))))
    p.put_uint(f.renderer + 0x4c, 0)
    actual = host('--keys-batch', rows)
    assert len(actual) == len(expected)
    count = 0
    for index, (result, native) in enumerate(zip(actual, expected)):
        assert len(result) == 3 and math.isclose(float(result[0]), native[0], rel_tol=3e-5, abs_tol=3e-5), (index, result, native)
        assert int(result[1]) == native[1] and int(result[2]) == native[2], (index, result, native)
        count += 3
    f.close()
    return count, len(expected)


def callbacks():
    f = ProtocolFixture()
    p = f.p
    rng = random.Random(0x423e30)
    rows, expected = [], []
    for index in range(48):
        phase, enabled = index % 2, (index // 2) % 2
        direct = rng.choice((0, 1, -1, 256, 257))
        results = [rng.choice((-1, 0, 1, -2, 256)) for _ in range(rng.randrange(9))]
        rows.append(' '.join(map(str, (phase, enabled, direct, len(results), *results))))
        obj = f.prepare()
        offset, entry, name = (0x34, 0x4240d0, 'post') if phase else (0x44, 0x423fd0, 'pre')
        f.group_buffer(obj, offset, tuple(range(1, len(results) + 1)))
        f.group_results = {i + 1: result for i, result in enumerate(results)}
        f.results[name] = direct & 0xffffffff
        p.mu.mem_write(obj + (0x55 if phase else 0x54), bytes([enabled]))
        observations = []
        for _ in range(2):
            f.calls.clear()
            result = f.call(entry, this=obj, args=(f.camera, f.support)) & 255
            events = [call for call in f.calls if call[0] == 'group']
            remaining = (p.uint(obj + offset + 8) - p.uint(obj + offset + 4)) // 8
            observations.extend((int(result != 0), remaining,
                                 sum(call[0] == name for call in f.calls), len(events)))
            for event in events:
                observations.extend(event[1:3])
        expected.append(observations)
    actual = host('--callbacks-batch', rows)
    assert len(actual) == len(expected)
    count = 0
    for index, (result, native) in enumerate(zip(actual, expected)):
        assert list(map(int, result)) == native, (index, result, native)
        count += len(native)
    f.close()
    return count, len(expected)


def main():
    key_count, key_cases = keys()
    callback_count, callback_cases = callbacks()
    total = key_count + callback_count
    print(f'PASS {total}/{total}: alpha keys {key_count}/{key_cases} cases; '
          f'callback states {callback_count}/{callback_cases} two-dispatch cases')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
