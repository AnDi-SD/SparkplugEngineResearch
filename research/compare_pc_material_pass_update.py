#!/usr/bin/env python3
"""Actual PC pass→layer→UV renderer chain vs reconstructed common classes."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_pass_update import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialApplyTests.exe'
    source=json.loads(subprocess.run([str(binary),'--pass',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:raise AssertionError(f'pass {mode}: source={source!r}; original={original!r}')
    print(f'PASS exact PC pass update {mode}: original layer order and UV device matrices, not GPU');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
