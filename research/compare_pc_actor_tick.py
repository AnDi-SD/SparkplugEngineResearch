#!/usr/bin/env python3
"""Bounded differential PC actor tick, including exact ordered boundary actions."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
from pc_instruction_emulator import ROOT, run_bounded
from probe_pc_actor_tick import ActorFixture
from compare_pc_san_reader import compare


def cases():
    result = []
    for mode in range(4):
        for reverse in (False, True):
            for progress, delta in ((0, 1), (.75, 1), (.75, 2), (1.75, 2), (.25, -2), (.25, 10), (.5, 0)):
                normalized = progress
                if mode == 1: normalized = math.fmod(progress, 1)
                if mode == 2:
                    normalized = math.fmod(progress, 2)
                    if normalized > 1: normalized = 2 - normalized
                sample = 4 * (1 - normalized if reverse else normalized)
                result.append((dict(mode=mode, reverse=reverse, progress=progress, sample=sample), delta))
    for fade in (2, 3, 4):
        for delta in (.125, .5, 1, 2):
            for stop in (False, True):
                result.append((dict(fade=fade, weight=.25, threshold=1, stop_after_fade=stop), delta))
    result.extend((config, delta) for config, delta in (
        ({'applies': False, 'advance': False}, 1), ({'applies': False}, 1),
        ({'running': False}, 1), ({'uses': 0}, 1),
        ({'transition': 2}, .5), ({'transition': 2}, 2), ({'transition': 2}, 2.5),
        ({'transition': 2, 'elapsed': 1}, .5),
        ({'multiplier': .5, 'actor_multiplier': 2}, 1),
        ({'multiplier': -1}, 1), ({'mode': 0, 'applies': False}, 5),
        ({'fade': 4, 'weight': .125, 'fade_rate': .125, 'threshold': .5}, 1),
        ({'fade': 3, 'weight': 0, 'stop_after_fade': True}, 0),
    ))
    return result


def config_with_defaults(config):
    defaults = dict(mode=1, reverse=False, progress=0, sample=0, weight=1, fade=0,
                    fade_rate=2, fade_out_rate=2, threshold=3, elapsed=0, transition=0,
                    applies=True, advance=True, running=True, uses=1, multiplier=1,
                    actor_multiplier=1, stop_after_fade=False, callback=True)
    defaults.update(config)
    return defaults


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', type=Path, required=True)
    args = parser.parse_args(argv)
    binary = args.portable.resolve()
    if not binary.is_relative_to(ROOT) or binary.name != 'SparkplugActorTests.exe':
        raise ValueError('Only workspace portable actor test binary is allowed')
    names = ('mode', 'reverse', 'progress', 'sample', 'weight', 'fade', 'fade_rate', 'fade_out_rate',
             'threshold', 'elapsed', 'transition', 'applies', 'advance', 'running', 'uses',
             'multiplier', 'actor_multiplier', 'stop_after_fade', 'callback')
    records = [(config_with_defaults(config), delta) for config, delta in cases()]
    lines = [str(len(records))]
    for config, delta in records:
        values = [delta] + [config[name] for name in names]
        lines.append(' '.join(str(int(value)) if isinstance(value, bool) else str(value) for value in values))
    output = subprocess.run([str(binary), '--probe-batch'], input='\n'.join(lines), text=True,
                            capture_output=True, timeout=10, check=True, cwd=ROOT).stdout
    if len(output) > 250000: raise ValueError('Portable output bound exceeded')
    portable = [json.loads(line) for line in output.splitlines()]
    if len(portable) != len(records): raise AssertionError('Case count mismatch')
    fixture = ActorFixture()
    count = 0
    for index, ((config, delta), actual) in enumerate(zip(records, portable)):
        fixture.prepare(**config, tags=(0, 1, 2, 3, 4))
        expected = fixture.tick(delta)
        try:
            count += compare(expected, actual, f'case[{index}]')
        except AssertionError:
            print('CONFIG', config, 'delta', delta, 'NATIVE', expected, 'PORTABLE', actual, flush=True)
            raise
    print(f'PASS {count}/{count}: {len(records)} original/portable actor tick and ordered action cases')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']: raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
