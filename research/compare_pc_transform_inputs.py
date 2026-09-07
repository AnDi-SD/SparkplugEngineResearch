#!/usr/bin/env python3
"""Deterministic original/portable two-input priority/cache differential checks."""
import argparse
from itertools import product
import json
from pathlib import Path
import subprocess
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_transform_inputs import InputFixture


def cases():
    bases = [[], [(0, 0, 10, 100)],
             [(0, 0, 10, 100), (1, 1, 20, 200)],
             [(0, 0, 20, 100), (1, 1, 20, 200)],
             [(0, 0, 0x80000000, 100), (1, 1, 0xffffffff, 200)],
             [(-1, -1, 10, 100), (1, 1, 20, 200)],
             [(0, 0, 10, 100), (-1, -1, 20, 200)]]
    result = []
    for slots, state, priority, exclusive in product(bases, range(3), (0, 10, 20, 0xffffffff), (False, True)):
        retained = [slot for slot in slots if slot[0] >= 0 and slot[0] != state]
        if not exclusive and len(retained) + 1 > 2:
            continue  # never attempt original third input
        uses = [sum(slot[0] == index for slot in slots) * 2 for index in range(3)]
        result.append((slots, uses, state, (state + 1) % 3, priority, exclusive))
    return result


def main(arguments):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', type=Path, required=True)
    args = parser.parse_args(arguments)
    samples = cases()
    rows = [str(len(samples))]
    for slots, uses, state, track, priority, exclusive in samples:
        physical = slots + [(-1, -1, 0xffffffff, 200)] * (2 - len(slots))
        values = [len(slots), *[value for slot in physical for value in slot],
                  *uses, state, track, priority, int(exclusive)]
        rows.append(' '.join(map(str, values)))
    portable = subprocess.run([str(args.portable.resolve()), '--probe-batch'],
                              input='\n'.join(rows) + '\n', text=True, capture_output=True, timeout=10)
    if portable.returncode:
        raise AssertionError(portable.stderr or portable.stdout)
    results = [json.loads(line) for line in portable.stdout.splitlines() if line.strip()]
    if len(results) != len(samples):
        raise AssertionError('portable result count mismatch')
    native = InputFixture()
    comparisons = 0

    def compare(a, b, path):
        nonlocal comparisons
        if isinstance(a, dict):
            if set(a) != set(b):
                raise AssertionError(path + ' dictionary keys differ')
            for key in a:
                compare(a[key], b[key], path + '.' + key)
        elif isinstance(a, list):
            if len(a) != len(b):
                raise AssertionError(path + ' sequence length differs')
            for index, (left, right) in enumerate(zip(a, b)):
                compare(left, right, path + '[' + str(index) + ']')
        else:
            comparisons += 1
            if a != b:
                raise AssertionError(f'{path}: original={a!r}, portable={b!r}')

    for index, ((slots, uses, state, track, priority, exclusive), result) in enumerate(zip(samples, results)):
        native.prepare(slots, uses)
        native.insert(state, track, priority, exclusive)
        compare(native.snapshot(), result, str(index))
    print(f'PASS {comparisons}/{comparisons}: {len(samples)} original/portable input priority/cache cases')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
