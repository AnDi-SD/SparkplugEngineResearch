#!/usr/bin/env python3
"""Original small Model/material/standard-layer graph vs compiled common source."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_links import main as native

def main(mode):
    if mode not in {'inline','repeat','clear','prebound'}:raise ValueError('explicit material graph case')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialSerializationTests.exe'
    result=subprocess.run([str(binary),'--material-graph',mode],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r};original={original!r}')
    print(f'PASS exact Model/Material/StdLayer {mode}: input/state/common nested output')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
