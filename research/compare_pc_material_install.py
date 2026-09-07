#!/usr/bin/env python3
"""Exact original/source material install and batch-state command captures."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_install import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialApplyTests.exe'
    source=json.loads(subprocess.run([str(binary),'--case',mode],capture_output=True,text=True,check=True,timeout=10).stdout)
    original=native(mode,True)
    if source!=original:
        for i,(left,right) in enumerate(zip(source[1],original[1])):
            if left!=right:raise AssertionError(f'{mode} row{i}: source={left!r}; native={right!r}')
        raise AssertionError('material install shape/header mismatch')
    print(f'PASS exact material install {mode}: original CPU ordering, not GPU/full frame');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
