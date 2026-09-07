#!/usr/bin/env python3
"""Complete native SAN loader output vs portable fields/owned binding/sampler.

This does NOT claim that the portable side implements the complete loader.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from pc_instruction_emulator import ROOT,run_bounded
from inspect_pc_san_keys import DEFAULT
from probe_pc_san_loader import load_asset
from compare_pc_san_reader import compare


def main(argv):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable',type=Path,required=True)
    args=parser.parse_args(argv);portable=args.portable.resolve()
    if not portable.is_relative_to(ROOT) or portable.name!='SparkplugSanReaderTests.exe':
        raise ValueError('only workspace portable test binary allowed')
    output=subprocess.run([str(portable),'--inspect-owned',str(DEFAULT/'bbush.san')],
                          cwd=ROOT,capture_output=True,text=True,timeout=10,check=True).stdout
    if len(output)>150000:raise ValueError('bounded output exceeded')
    actual=json.loads(output);expected=load_asset('bbush.san',quiet=True)
    # usedPools is parser-derived metadata on native side, not a captured
    # runtime field. Existing key-buffer tests cover that independently.
    del actual['usedPools'];del expected['usedPools']
    count=compare(expected,actual)
    print(f'PASS {count}/{count}: whole native SAN load vs portable fields/owned bindings/PRS')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
