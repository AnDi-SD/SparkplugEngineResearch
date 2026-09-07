#!/usr/bin/env python3
"""Bit-exact native/portable task-timer states under bounded scalar inputs."""
import argparse
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
from pc_instruction_emulator import ROOT, run_bounded
from probe_pc_task_timer import TimerFixture


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', required=True, type=Path)
    args = parser.parse_args(argv)
    portable = args.portable.resolve()
    if not portable.is_relative_to(ROOT) or portable.name != 'SparkplugTaskTimerTests.exe':
        raise ValueError('only bounded workspace timer test executable allowed')
    cases = []
    for operation in 'USPR':
        for active in (0, 1):
            for relative in (0, 1):
                for linked in (0, 1):
                    for current, ticks in ((0, 250), (0xffffff00, 16), (16, 0), (0, 0xffffffff)):
                        cases.append([operation, active, relative, current, 1000, 250,
                                      .625, linked, 4321, .875, ticks, 1])
    rng = random.Random(0x1acd36e2)
    for _ in range(200):
        cases.append([rng.choice('USPR'), rng.randrange(2), rng.randrange(2),
                      rng.randrange(1 << 32), rng.randrange(1 << 32), rng.randrange(1 << 32),
                      rng.choice([0., -.25, .5, 64.]), rng.randrange(2), rng.randrange(1 << 32),
                      rng.choice([-.5, 0., 1., 1024.]), rng.randrange(1 << 32), rng.choice([1, 2, 3, 1000, 65535])])
    data = '\n'.join(' '.join(map(str, row)) for row in cases) + '\n'
    output = subprocess.run([str(portable), '--batch'], input=data, capture_output=True,
                            text=True, cwd=ROOT, timeout=10, check=True).stdout
    if len(output) > 200000:
        raise ValueError('bounded timer output')
    actual = json.loads(output)
    if len(actual) != len(cases):
        raise AssertionError('portable timer case count')
    f = TimerFixture()
    p = f.p
    source = f.create()
    timer = f.create()
    checked = 0
    for index, (operation, active, relative, current, start, paused, delta, linked,
                source_current, source_delta, ticks, divisor) in enumerate(cases):
        p.mu.mem_write(timer + 0x18, bytes([active, relative]))
        for offset, value in ((0x1c, current), (0x20, start), (0x24, paused),
                              (0x2c, source if linked else 0)):
            p.put_uint(timer + offset, value)
        p.put_floats(timer + 0x28, [delta])
        p.put_uint(source + 0x1c, source_current)
        p.put_floats(source + 0x28, [source_delta])
        f.time(ticks, divisor)
        f.call({'U': 0x450750, 'S': 0x450840, 'P': 0x4506d0, 'R': 0x4506e0}[operation], this=timer)
        expected = [p.uint(timer + 0x18) & 255, (p.uint(timer + 0x18) >> 8) & 255,
                    *[p.uint(timer + offset) for offset in (0x1c, 0x20, 0x24, 0x28)]]
        if expected != actual[index]:
            raise AssertionError(f'case{index} {cases[index]} expected{expected} actual{actual[index]}')
        checked += len(expected)
    f.close()
    print(f'PASS {checked}/{checked}: {len(cases)} original/portable task-timer cases, exact delta bits')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
