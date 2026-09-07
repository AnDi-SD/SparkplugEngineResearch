#!/usr/bin/env python3
"""Bounded original/portable owned SAN registry lifetime differential check."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from pc_instruction_emulator import ROOT, run_bounded
from inspect_pc_san_keys import DEFAULT
from probe_pc_san_registry import shared_asset
from compare_pc_san_reader import compare


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', type=Path, required=True)
    parser.add_argument('--asset', choices=['bbush.san', 'bflower.san'], default='bbush.san')
    args = parser.parse_args(argv)
    portable = args.portable.resolve()
    if not portable.is_relative_to(ROOT) or portable.name != 'SparkplugSanReaderTests.exe':
        raise ValueError('Only workspace portable test binary allowed')
    output = subprocess.run([str(portable), '--registry-lifetime', str(DEFAULT / args.asset)],
                            cwd=ROOT, capture_output=True, text=True, timeout=10, check=True).stdout
    if len(output) > 100000:
        raise ValueError('Bounded registry output exceeded')
    actual = json.loads(output)
    expected = shared_asset(args.asset, quiet=True)
    count = compare(expected, actual)
    print(f'PASS {count}/{count}: {args.asset} original/portable registry refs, release and reload')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
