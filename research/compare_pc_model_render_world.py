#!/usr/bin/env python3
"""Full finite model->RenderNode state sequences against original PC methods.

Native objects/append/world/getters/cache preparation execute unchanged in the
bounded guest. Only mesh payload records and the device matrix leaf are fixtures.
This compares CPU reconstruction, not in-game rendering or bit-exact x87 results.
"""
import argparse
import json
import math
from pathlib import Path
import random
import struct
import subprocess
import sys
from pc_instruction_emulator import ROOT, run_bounded
from probe_pc_render_node_ownership import OwnedFixture


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def cases():
    rng = random.Random(0x763277db)
    rows = []
    for index in range(32):
        spheres = [[*[rng.uniform(-4, 4) for _ in range(3)], rng.uniform(.2, 3)]
                   for _ in range(3)]
        if index < 5:
            spheres[0][3] = (0., .0005, .001, .01, 8.)[index]
        position = [rng.uniform(-20, 20) for _ in range(3)]
        scale = [rng.choice((-1, 1)) * rng.uniform(.25, 4) for _ in range(3)]
        angle = rng.uniform(-3, 3)
        sine, cosine = math.sin(angle), math.cos(angle)
        orientation = [cosine, sine, 0, -sine, cosine, 0, 0, 0, 1]
        rows.append([f32(v) for v in [*[v for s in spheres for v in s],
                                      *position, *scale, *orientation]])
    return rows


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', required=True, type=Path)
    args = parser.parse_args(argv)
    portable = args.portable.resolve()
    if not portable.is_relative_to(ROOT) or portable.name != 'SparkplugRenderNodeTests.exe':
        raise ValueError('only workspace render-node test executable allowed')
    rows = cases()
    output = subprocess.run([str(portable), '--runtime-batch'],
                            input='\n'.join(' '.join(map(str, row)) for row in rows) + '\n',
                            capture_output=True, text=True, cwd=ROOT, timeout=10, check=True).stdout
    if len(output) > 200000:
        raise ValueError('bounded differential output')
    actual = [json.loads(line) for line in output.splitlines()]
    if len(actual) != len(rows):
        raise AssertionError('case count mismatch')
    f = OwnedFixture()
    p = f.p
    p.mu.mem_map(0x340d0000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
    table = p.allocate(0x40)
    p.put_uint(f.renderer + 0x18, table)
    p.put_uint(table + 0x38, 0x340d0010)
    p.seams[0x340d0010] = lambda p: p.fixture_return(8, eax=1)  # explicit GPU boundary
    checked = 0
    for index, row in enumerate(rows):
        node = f.node()
        first, second = f.mesh_record(row[:4]), f.mesh_record(row[4:8])
        for mesh in (first, second):
            model = f.model()
            f.call(0x479e20, this=model, args=(mesh,))
            f.append_model(node, model)
        expected = list(p.floats(node + 0xc8, 8))
        p.put_floats(node + 0x20, row[12:15])
        p.put_floats(node + 0x30, row[15:18])
        p.put_floats(node + 0x40, row[18:27])
        f.call(0x4250f0, this=node, args=(1,))
        expected.extend((*p.floats(node + 0xc8, 8), *p.floats(node + 0x1b8, 3),
                         p.uint(node + 0x134)))
        f.call(0x4248d0, this=node + 0xb4)
        expected.extend((*p.floats(node + 0x138, 16), *p.floats(node + 0x178, 16),
                         p.uint(node + 0x134)))
        p.put_floats(first + 0x18, row[8:12])
        p.put_uint(node + 0xb0, p.uint(node + 0xb0) | 2)
        f.call(0x4250f0, this=node, args=(0,))
        expected.extend((*p.floats(node + 0xc8, 8), p.uint(node + 0x134)))
        f.call(0x4250f0, this=node, args=(1,))
        expected.extend((*p.floats(node + 0xc8, 8), p.uint(node + 0x134)))
        if len(expected) != len(actual[index]) or not all(
                math.isclose(a, b, rel_tol=3e-5, abs_tol=3e-5)
                for a, b in zip(actual[index], expected)):
            raise AssertionError(f'case{index} input{row} expected{expected} actual{actual[index]}')
        checked += len(expected)
    f.close()
    print(f'PASS {checked}/{checked}: {len(rows)} original/portable model-render-world sequences, tolerance3e-5')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
