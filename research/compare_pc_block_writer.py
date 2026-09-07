#!/usr/bin/env python3
"""Compare actual PC nested block output to compiled reconstruction, 18 cases."""
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]


def outputs(command):
    result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=30)
    if result.returncode:raise AssertionError(result.stdout+'\n'+result.stderr)
    lines=[line for line in result.stdout.splitlines() if line.startswith('BLOCK_OUTPUTS ')]
    if len(lines)!=1:raise AssertionError('missing/unexpected output record')
    return json.loads(lines[0].split(' ',1)[1])


def main():
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugDataBlockWriterTests.exe'
    native=outputs([sys.executable,str(ROOT/'research/probe_pc_block_writer.py'),'success'])
    portable=outputs([str(binary),'--emit'])
    if len(native)!=18 or native!=portable:raise AssertionError('native/portable block writer mismatch')
    count=sum(len(bytes.fromhex(item)) for item in native)
    print(f'PASS 18/18 block writer outputs, {count} exact bytes; native quirks/host guards tested separately')
    return 0


if __name__=='__main__':raise SystemExit(main())
