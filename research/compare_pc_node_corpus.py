#!/usr/bin/env python3
"""Compiled whole loader vs staged original outer on unchanged Node-only SMO."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_node_corpus import main as native,ASSETS

def main(name):
    if name not in ASSETS:raise ValueError('explicit tiny unchanged Node corpus')
    if name=='gameover.smo':raise ValueError('Disabled: original outer reached100k guard; never retry/resume this capped probe')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugNodeSerializationTests.exe'
    result=subprocess.run([str(binary),'--asset',str(ROOT/'local-data/pc-pristine/Media/Menus'/name)],
                          capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=native(name,True,scene_ready=name=='gameover.smo')
    if original!=source:raise AssertionError(f'{name}: original={original!r} source={source!r}')
    print(f'PASS {len(source)} exact Node records: {name}; names,flags,local/world float bits,tree edges; native outer != whole422B50 evidence')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
