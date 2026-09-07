#!/usr/bin/env python3
"""Original raw material-to-DX mapping and layer prefix vs common C++ core."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_apply import main as native
def main(mode):
    if mode=='lighting0':raise ValueError('lighting boundary not yet restored')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugDXRenderStateTests.exe'
    source=json.loads(subprocess.run([str(binary),'--material',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r}; original={original!r}')
    print(f'PASS exact PC material state {mode}; layered native cache ordering, no GPU');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
