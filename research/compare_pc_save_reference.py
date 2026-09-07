#!/usr/bin/env python3
"""Original versus reconstructed inline/repeated/null resource reference."""
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]


def output(command):
    result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=30)
    if result.returncode:raise AssertionError(result.stdout+'\n'+result.stderr)
    rows=[s for s in result.stdout.splitlines() if s.startswith('REFERENCE_OUTPUT_HEX ')]
    if len(rows)!=1:raise AssertionError('missing reference output')
    return bytes.fromhex(rows[0].split(' ',1)[1])


def main():
    native=output([sys.executable,str(ROOT/'research/probe_pc_save_reference.py'),'success'])
    portable=output([str(ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSaveReferenceTests.exe'),'--emit'])
    if len(native)!=75 or native!=portable:raise AssertionError('reference output mismatch')
    print('PASS inline/repeated/null references:75 exact native/portable bytes; unsafe failure semantics tested separately')
    return 0


if __name__=='__main__':raise SystemExit(main())
