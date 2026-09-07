#!/usr/bin/env python3
"""PC material color consumer vs source; NEVER factory41A580/clone41AF70."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_color import main as native

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialColorTests.exe'
    source=json.loads(subprocess.run([str(binary),'--material-color',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r}; original={original!r}')
    print(f'PASS exact material color {mode}: declared input consumer, not factory/full-load');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
