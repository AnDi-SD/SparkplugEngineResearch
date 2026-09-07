#!/usr/bin/env python3
"""Exact full RFX file metadata result; native failure leaks asserted separately."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_rfx_file import main as original
def main(mode):
    native=original(mode,True)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugRFXFileTests.exe'
    source=json.loads(subprocess.run([str(binary),'--case',mode],capture_output=True,text=True,check=True,timeout=10).stdout)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact PC RFX full file/regex/template',mode);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
