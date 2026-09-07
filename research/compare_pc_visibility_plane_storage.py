#!/usr/bin/env python3
"""Fixed-seed operation sequences, original x86 versus partial C++ plane storage.

Compares defined raw equation/flag/size/capacity/active-count fields, not native
indeterminate padding or host pointers. Fresh guest per eight bounded cases.
"""
from pathlib import Path
import random
import subprocess
import sys
from pc_instruction_emulator import run_bounded, ROOT
from probe_pc_visibility_plane_storage import PlaneFixture

EXE = ROOT / '.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugVisibilityPlaneStorageTests.exe'


def sequences(start, count):
    values = []
    for index in range(start, start + count):
        rng = random.Random(0x46b720 + index)
        operations = []
        for step in range(8):
            if step in (0, 1, 3, 5, 7):
                size = (3, 4, 0, 5, 7)[(0, 1, 3, 5, 7).index(step)] if index % 4 == 0 else rng.randrange(13)
                bits = [rng.getrandbits(32) for _ in range(4)]
                if step == 0:
                    bits[:2] = [0x7fc12345, 0x80000000]
                operations.append([0, size, *bits, rng.choice((0, 1, 7, 255))])
            elif step == 6 and index % 3 == 0:
                operations.append([2, 0])
            else:
                operations.append([1, rng.choice((0, 1, 7, 255, 256, 257))])
        operations.append([3, 0])  # fresh vector copy, not unresolved assignment
        values.append(operations)
    return values


def main(start, count):
    cases = sequences(start, count)
    text = '\n'.join(str(len(ops)) + '\n' + '\n'.join(' '.join(map(str, op)) for op in ops) for ops in cases) + '\n'
    source = subprocess.run([str(EXE), 'batch'], input=text, text=True,
                            capture_output=True, timeout=10, check=True)
    actual_source = [list(map(int, line.split(','))) for line in source.stdout.splitlines()]
    f = PlaneFixture(); p = f.p; observed = []; fields = 0
    for ops in cases:
        obj = f.record()
        for op in ops:
            if op[0] == 0:
                f.resize(obj, op[1], tuple(op[2:]))
            elif op[0] == 1:
                f.call(0x46adc0, this=obj, args=(op[1],))
            elif op[0] == 2:
                f.call(0x45ea00, this=obj)
            else:
                copy = p.allocate(20); p.mu.mem_write(copy, b'\xdd' * 20)
                f.call(0x45e530, this=copy, args=(obj,))
                f.call(0x45ea00, this=obj)
                obj = copy
            begin, size, capacity = f.state(obj)
            row = [p.uint(obj), size, capacity, p.uint(obj + 16)]
            for index in range(size):
                at = begin + index * 20
                row.extend(f.words(at, 0, 16)); row.append(p.uint(at + 16) & 255)
            observed.append(row); fields += len(row)
        f.call(0x45ea00, this=obj)
    if observed != actual_source:
        for index, (a, b) in enumerate(zip(observed, actual_source)):
            if a != b:
                raise AssertionError(f'case{start + index // 9}/operation{index % 9}: native={a} source={b}')
        raise AssertionError('output count differs')
    if set(f.allocations) != set(f.freed):
        raise AssertionError('owned native plane allocation leak')
    print(f'PASS {fields}/{fields}: PC plane-storage differential cases{start}..{start + count - 1}, {len(observed)} operations')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(int(sys.argv[2]), int(sys.argv[3])))
    for start in range(0, 64, 8):
        code = run_bounded(Path(__file__), (str(start), '8'))
        if code:
            raise SystemExit(code)
