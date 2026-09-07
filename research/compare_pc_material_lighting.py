#!/usr/bin/env python3
"""Original PC lighting/color-source wrappers vs common reconstructed core."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_lighting import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugDXRenderStateTests.exe'
    source=json.loads(subprocess.run([str(binary),'--lighting',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:
        for index,(left,right) in enumerate(zip(source[1],original[1])):
            if left!=right:raise AssertionError(f'{mode} row{index}: source={left!r}; original={right!r}')
        raise AssertionError('lighting capture shape/header mismatch')
    print(f'PASS exact PC lighting {mode}: color/cache mutation and device call order, not GPU');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
