#!/usr/bin/env python3
"""PC prebound links / DX frame gate vs shared source, not factory proof."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_material_color_links import main as links
from probe_pc_material_color_frame import main as frame

def main(kind,mode):
    if kind not in ('links','frame'):raise ValueError('explicit consumer group')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialColorTests.exe'
    source=json.loads(subprocess.run([str(binary),'--'+kind,mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=(links if kind=='links' else frame)(mode,True)
    if source!=original:raise AssertionError(f'{kind}/{mode}: source={source!r}; original={original!r}')
    print(f'PASS exact material color {kind}/{mode}: original consumer vs common source, factoryExcluded=true');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
