#!/usr/bin/env python3
"""Exact original/source full pass-state and final blend dispatch captures."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_pass_states import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialApplyTests.exe'
    source=json.loads(subprocess.run([str(binary),'--pass-states',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:
        for i,(left,right) in enumerate(zip(source[1],original[1])):
            if left!=right:
                for j,(a,b) in enumerate(zip(left,right)):
                    if a!=b:raise AssertionError(f'pass states {mode} row{i}/field{j}: source={a!r}; original={b!r}')
        raise AssertionError('pass state shape/header mismatch')
    print(f'PASS exact PC pass states {mode}: full64-state dispatch and intermediate caches, not GPU');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
