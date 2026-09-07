#!/usr/bin/env python3
"""Exact original/source PC parser normalization capture, bounded inputs only."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_parser import main as original

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugParserTests.exe'
    source=json.loads(subprocess.run([str(binary),'--case',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    native=original(mode,True)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact PC parser',mode);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
