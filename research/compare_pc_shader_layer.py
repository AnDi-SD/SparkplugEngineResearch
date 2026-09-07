#!/usr/bin/env python3
"""Original PC DXShaderLayer lifetime/state vs rebuilt common source."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_shader_layer import main as native

def main(mode):
    if mode=='reject':raise ValueError('native codec rejection is separate, not source stream-parity proof')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugShaderLayerTests.exe'
    source=json.loads(subprocess.run([str(binary),'--shader',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r}; original={original!r}')
    print(f'PASS exact shader-layer {mode}: factory/clone/storage/stubs, not codec/backend');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
