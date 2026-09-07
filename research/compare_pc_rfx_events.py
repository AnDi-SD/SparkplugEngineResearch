#!/usr/bin/env python3
"""Exact engine callback captures using the same decoded XML events."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_rfx_events import main as original,CASES

def event_input(events):
    lines=['-']
    for start,tag,attributes in events:
        lines.append(f'{"S" if start else "E"} {tag.encode().hex()} {len(attributes)}')
        for key,value in attributes.items():lines.append(f'{key.encode().hex()} {value.encode().hex() or "-"}')
    return '\n'.join(lines)+'\n'

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugEffectTemplateTests.exe'
    native=original(mode,True)
    source=json.loads(subprocess.run([str(binary),'--events',mode],input=event_input(CASES[mode]),capture_output=True,text=True,timeout=10,check=True).stdout)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact PC RFX events',mode);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
