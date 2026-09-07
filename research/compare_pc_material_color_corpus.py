#!/usr/bin/env python3
"""Hash-checked pristine54-byte object payload vs common source codec."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_color_corpus import main as native

def main():
    original=native(True)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialColorTests.exe'
    source=json.loads(subprocess.run([str(binary),'--corpus',original[1]],capture_output=True,text=True,timeout=10,check=True).stdout)
    if source!=original:raise AssertionError(f'source={source!r}; original={original!r}')
    print('PASS exact pristine ColorController codec/runtime on declared type0 state; factoryExcluded=true');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
