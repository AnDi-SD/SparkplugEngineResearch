#!/usr/bin/env python3
"""Whole original manager generating miss vs restored engine at SDK/device ABI."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_shader_generation import main as original,CASES

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugShaderCompileTests.exe'
    native=original(mode,True)
    source=json.loads(subprocess.run([str(binary),'--generation',mode],input=' '.join(map(str,CASES[mode]))+'\n',capture_output=True,text=True,timeout=10,check=True).stdout)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact whole PC shader generating miss',mode);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
