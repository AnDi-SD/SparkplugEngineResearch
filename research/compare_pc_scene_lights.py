#!/usr/bin/env python3
"""Original/portable PC light selection and complete eight-slot cache history."""
import argparse
import json
from pathlib import Path
import random
import subprocess
import sys
from pc_instruction_emulator import ROOT, run_bounded
from probe_pc_scene_lights import LightSceneFixture


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', required=True, type=Path)
    args = parser.parse_args(argv)
    portable = args.portable.resolve()
    if not portable.is_relative_to(ROOT) or portable.name != 'SparkplugLightManagerTests.exe':
        raise ValueError('only bounded workspace light-manager test executable allowed')
    rows = [['A', i] for i in range(11)] + [['A', 1], ['R', 1], ['A', 8], ['R', 8],
            ['R', 10], ['R', 9], ['A', 10], ['Z'], ['A', 0]]
    rng = random.Random(0x6fcd243a)
    for _ in range(120):
        operation = rng.choice('AARZ')
        rows.append([operation] if operation == 'Z' else [operation, rng.randrange(11)])
    for kind in range(5):
        for flags in (0, 0x100, 0x200, 0x300):
            for enabled in (0, 1):
                for shadow in (0, 1):
                    for exclude in (0, 1):
                        for distance in (5., 5.25):
                            rows.append(['E', kind, flags, enabled, shadow, exclude, 3.,
                                         distance, 0., 0., 0., 0., 0., 2.])
    for _ in range(80):
        rows.append(['E', rng.randrange(5), 0x100, 1, rng.randrange(2), rng.randrange(2),
                     rng.uniform(.1, 10), *[rng.uniform(-10, 10) for _ in range(6)], rng.uniform(.1, 5)])
    data = '\n'.join(' '.join(map(str, row)) for row in rows) + '\n'
    output = subprocess.run([str(portable), '--batch'], input=data, capture_output=True,
                            text=True, cwd=ROOT, timeout=10, check=True).stdout
    if len(output) > 200000: raise ValueError('bounded light batch output')
    actual = [json.loads(line) for line in output.splitlines()]
    if len(actual) != len(rows): raise AssertionError('portable light case count')
    f = LightSceneFixture()
    p = f.p
    lights = [f.light() for _ in range(11)]
    for light in lights[9:]: p.put_uint(light + 0xc0, 3)
    cache, node = p.allocate(0x28), f.render_node()
    f.call(0x490b20, this=cache)
    ids = {light: i for i, light in enumerate(lights)}
    ids[0] = -1
    checked = 0
    for index, row in enumerate(rows):
        operation = row[0]
        if operation == 'E':
            _, kind, flags, enabled, shadow, exclude, radius, *positions = row
            light = lights[0]
            p.put_uint(light + 0xc0, kind); p.put_uint(light + 0xb0, flags)
            p.mu.mem_write(light + 0xec, bytes((shadow, enabled)))
            p.mu.mem_write(node + 0x121, bytes((exclude,)))
            p.put_floats(light + 0xe0, (radius,))
            p.put_floats(light + 0x74, positions[:3]); p.put_floats(node + 0xd8, positions[3:])
            expected = [f.call(0x46a850, args=(light, node)) & 255]
        else:
            if operation == 'Z':
                p.put_uint(cache + 0x20, 0); p.put_uint(cache + 0x24, 0)
            else:
                # After first safe append the native protection constant has
                # resolved; assert capacity before any possible ninth attempt.
                if p.uint(cache + 0x24) >= 8 and p.uint(0x13b3688) != 8:
                    raise AssertionError('native ordinary-light capacity invariant')
                f.call(0x490b50 if operation == 'A' else 0x490ba0, this=cache, args=(lights[row[1]],))
            expected = [p.uint(cache + 0x24), ids[p.uint(cache + 0x20)],
                        *[ids[p.uint(cache + i * 4)] for i in range(8)]]
        if actual[index] != expected:
            raise AssertionError(f'case{index} {row} expected{expected} actual{actual[index]}')
        checked += len(expected)
    for light in lights: f.call(0x435430, this=light, args=(1,))
    f.call(0x4255d0, this=node, args=(1,))
    f.close()
    print(f'PASS {checked}/{checked}: {len(rows)} native/portable light eligibility/cache transitions')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']: raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
