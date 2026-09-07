#!/usr/bin/env python3
"""Compare full PC SAN object reader/sampler with portable original classes."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_san_reader import read_asset,DEFAULT

def compare(a,b,where='root'):
    if isinstance(a,dict):
        if set(a)!=set(b):raise AssertionError(where+' fields')
        return sum(compare(a[key],b[key],where+'.'+key) for key in a)
    if isinstance(a,list):
        if len(a)!=len(b):raise AssertionError(where+' length')
        return sum(compare(x,y,where+f'[{i}]') for i,(x,y) in enumerate(zip(a,b)))
    if isinstance(a,float):
        if not math.isfinite(a) or not math.isfinite(b) or not math.isclose(a,b,rel_tol=4e-5,abs_tol=4e-5):
            raise AssertionError(f'{where}: native={a}, portable={b}')
    elif a!=b:raise AssertionError(f'{where}: native={a}, portable={b}')
    return 1

def main(argv):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable',type=Path,required=True)
    parser.add_argument('--asset',choices=['bbush.san','bflower.san','barrel.san','bw.san'],default='bbush.san')
    args=parser.parse_args(argv)
    portable=args.portable.resolve()
    if not portable.is_relative_to(ROOT) or portable.name!='SparkplugSanReaderTests.exe':
        raise ValueError('Only the workspace portable test binary is allowed')
    output=subprocess.run([str(portable),'--inspect',str(DEFAULT/args.asset)],
        cwd=ROOT,capture_output=True,text=True,timeout=10,check=True).stdout
    if len(output)>150000:raise ValueError('portable output exceeded bound')
    actual=json.loads(output)
    expected=read_asset(args.asset,capture_samples=True,quiet=True)
    count=compare(expected,actual)
    print(f'PASS {count}/{count}: {args.asset} full original/portable reader + PRS comparison')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
