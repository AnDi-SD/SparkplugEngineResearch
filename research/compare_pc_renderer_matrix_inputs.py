#!/usr/bin/env python3
"""Compare original matrix setter/cache/COM ordering and raw getters."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_renderer_matrix_inputs import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugRendererMatrixInputsTests.exe'
    source=json.loads(subprocess.run([str(binary),'--case',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:raise AssertionError(f'matrix inputs/{mode}: source={source!r}; original={original!r}')
    print(f'PASS exact PC renderer matrix inputs/{mode}; no live device or startup claim');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
