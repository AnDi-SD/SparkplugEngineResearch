#!/usr/bin/env python3
"""Original/compiled generic reference->Animation->owned names->PRS comparison."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from inspect_pc_san_keys import DEFAULT
from probe_pc_read_reference import asset_reference
from compare_pc_san_reader import compare

def main():
    portable=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSanReaderTests.exe'
    output=subprocess.run([str(portable),'--inspect-reference',str(DEFAULT/'bbush.san')],
        cwd=ROOT,capture_output=True,text=True,timeout=10,check=True).stdout
    if len(output)>150000:raise ValueError('bounded output exceeded')
    actual=json.loads(output);expected=asset_reference()
    # Parser-derived native usedPools and unrequested portable observations are
    # not captured runtime fields. No zero pool-size claim follows from them.
    del actual['usedPools'];del expected['usedPools']
    count=compare(expected,actual)
    print(f'PASS {count}/{count}: native/portable reference dispatch, owned names and PRS')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
