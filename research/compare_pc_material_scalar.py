#!/usr/bin/env python3
"""Bounded actual PC scalar material vs compiled source state and complete bytes."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_scalar import main as native

def main(mode):
    if mode not in {'empty','values','repeat','null-controller','pass-only'}:raise ValueError('explicit differential; full logo native probe is disabled after cap')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialSerializationTests.exe'
    result=subprocess.run([str(binary),'--material',mode],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r};original={original!r}')
    print(f'PASS exact PC Material {mode}: input/result/cursor/rawstate/passcount/output')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
