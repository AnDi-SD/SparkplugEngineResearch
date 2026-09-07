#!/usr/bin/env python3
"""Tiny standard-layer source/native comparison; never the disabled logo case."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_layers import main as native

def main(mode):
    if mode not in {'default','states','repeat','legacy','uv','uv-zero','uv-repeat'}:raise ValueError('explicit standard-layer mode')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialSerializationTests.exe'
    result=subprocess.run([str(binary),'--material-layer',mode],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r};original={original!r}')
    print(f'PASS exact standard-layer {mode}: input,73 raw state bytes and complete material output')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
