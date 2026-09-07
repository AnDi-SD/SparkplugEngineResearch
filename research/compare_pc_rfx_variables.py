#!/usr/bin/env python3
"""Exact original/source semantic RFX variable payloads; lifetime checked native."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_rfx_variables import main as original,CASES
from compare_pc_rfx_events import event_input

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugEffectTemplateTests.exe'
    native=original(mode,True)
    source=json.loads(subprocess.run([str(binary),'--variables',mode],input=event_input(CASES[mode]),capture_output=True,text=True,timeout=10,check=True).stdout)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact PC RFX variables',mode);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
