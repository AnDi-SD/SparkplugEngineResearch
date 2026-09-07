#!/usr/bin/env python3
"""Original PC texture/sampler mapping vs restored spDXRenderer method."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_state_map import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugDXRenderStateTests.exe'
    source=json.loads(subprocess.run([str(binary),'--texture',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:
        for index,(left,right) in enumerate(zip(source[1],original[1])):
            if left!=right:raise AssertionError(f'{mode} row{index}: source={left!r}; original={right!r}')
        raise AssertionError('texture capture count/header mismatch')
    print(f'PASS exact PC texture map {mode}: native command/raw/device-cache order, not GPU');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
