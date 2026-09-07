#!/usr/bin/env python3
"""Exact whole linked pipeline, source XML decoder is an explicit boundary."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_rfx_pipeline import main as original,events_for,document
from compare_pc_rfx_events import event_input
def main(mode):
    native=original(mode,True)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugRFXFileTests.exe'
    text=document(mode).hex()+event_input(events_for(mode))[1:]
    source=json.loads(subprocess.run([str(binary),'--pipeline',mode],input=text,capture_output=True,text=True,check=True,timeout=10).stdout)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact linked PC file/XML/compiler pipeline',mode);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
