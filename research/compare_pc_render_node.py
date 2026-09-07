#!/usr/bin/env python3
"""Finite-input differential for reconstructed PC render-node geometry math.

Native code stays in the bounded guest; the only native rendering leaf here is
an explicit matrix-argument recorder, never a graphics device. Float tolerance
is stated, not sold as bit-identical x87 arithmetic.
"""
import argparse
import json
import math
from pathlib import Path
import random
import subprocess
import sys

from pc_instruction_emulator import ROOT, run_bounded
from probe_pc_render_node_runtime import RuntimeFixture


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', required=True, type=Path)
    args = parser.parse_args(argv)
    portable = args.portable.resolve()
    if not portable.is_relative_to(ROOT) or portable.name != 'SparkplugRenderNodeTests.exe':
        raise ValueError('only bounded workspace render-node test executable allowed')
    rng = random.Random(0x603625d0)
    rows = []
    eps = 0.0010000000474974513
    for first, other in (
        ((0, 0, 0, 0), (3, 4, 5, 1)),
        ((1, 2, 3, 10), (2, 3, 4, 1)),
        ((1, 2, 3, 1), (2, 3, 4, 10)),
        ((0, 0, 0, 2), (2, 0, 0, 2)),
        ((0, 0, 0, 1), (0, 0, 0, 1)),
        ((0, 0, 0, 1), (.0005, .0005, .0005, 1)),
        ((0, 0, 0, 1), (1, 0, 0, eps)),
        ((0, 0, 0, 1), (1, 0, 0, eps / 2))):
        rows.append(['M', *first, *other])
    for _ in range(40):
        rows.append(['M', *[rng.uniform(-10, 10) for _ in range(3)], rng.uniform(.01, 8),
                     *[rng.uniform(-10, 10) for _ in range(3)], rng.uniform(.01, 8)])
    for _ in range(32):
        angle = rng.uniform(-3, 3)
        sine, cosine = math.sin(angle), math.cos(angle)
        sphere = [*[rng.uniform(-8, 8) for _ in range(3)], rng.uniform(.1, 5)]
        position = [rng.uniform(-30, 30) for _ in range(3)]
        scale = [rng.choice((-1, 1)) * rng.uniform(.25, 4) for _ in range(3)]
        rotation = [cosine, sine, 0, -sine, cosine, 0, 0, 0, 1]
        rows.append(['W', *sphere, *position, *scale, *rotation])
        matrix = [scale[r] * rotation[r * 3 + c] if r < 3 and c < 3 else
                  position[c] if r == 3 and c < 3 else 1 if r == c == 3 else 0
                  for r in range(4) for c in range(4)]
        rows.append(['B', *sphere, *matrix])
        planes = [[rng.uniform(-1, 1) for _ in range(3)] + [rng.uniform(-5, 5)] for _ in range(6)]
        rows.append(['C', *sphere, *[v for plane in planes for v in plane], rng.randrange(2)])
    data = '\n'.join(' '.join(map(str, row)) for row in rows) + '\n'
    output = subprocess.run([str(portable), '--batch'], input=data, capture_output=True,
                            text=True, cwd=ROOT, timeout=10, check=True).stdout
    if len(output) > 200000:
        raise ValueError('bounded render-node batch output')
    actual = [json.loads(line) for line in output.splitlines()]
    if len(actual) != len(rows):
        raise AssertionError('portable render-node case count')
    f = RuntimeFixture()
    p, n = f.p, f.node
    target, other = p.allocate(16), p.allocate(16)
    checked = 0
    for index, row in enumerate(rows):
        operation, values = row[0], row[1:]
        if operation == 'M':
            p.put_floats(target, values[:4]); p.put_floats(other, values[4:])
            f.call(0x463670, this=target, args=(other,))
            expected = p.floats(target, 4)
        elif operation == 'W':
            p.put_floats(n + 0xc8, values[:4])
            p.put_floats(n + 0x20, values[4:7]); p.put_floats(n + 0x30, values[7:10])
            p.put_floats(n + 0x40, values[10:19]); p.put_uint(n + 0xb0, 0x70a01)
            f.call(0x4250f0, this=n, args=(0,))
            f.call(0x4248d0, this=n + 0xb4)
            expected = (*p.floats(n + 0xd8, 4), *p.floats(n + 0x138, 16), *p.floats(n + 0x178, 16))
        elif operation == 'B':
            f.install_renderables((values[:4],))
            p.put_floats(n + 0x138, values[4:])
            f.call(0x469820, this=n + 0xb4)
            expected = p.floats(n + 0xd8, 4)
        else:
            p.put_floats(n + 0xd8, values[:4])
            p.put_floats(f.camera + 0x1c4, values[4:28])
            p.mu.mem_write(n + 0x130, bytes((int(values[28]),)))
            expected = (f.call(0x424840, this=n, args=(f.camera,)) & 255,)
        if len(actual[index]) != len(expected) or not all(
            math.isclose(a, b, rel_tol=3e-5, abs_tol=3e-5)
            for a, b in zip(actual[index], expected)):
            raise AssertionError(f'case{index} {row} expected{expected} actual{actual[index]}')
        checked += len(expected)
    f.close()
    print(f'PASS {checked}/{checked}: {len(rows)} original/portable render-node math cases, tolerance3e-5')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
