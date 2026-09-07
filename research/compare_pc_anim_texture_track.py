#!/usr/bin/env python3
"""Exact staged PC texture-track native execution vs rebuilt common source."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_anim_texture_track import main as native

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialControllerTests.exe'
    result=subprocess.run([str(binary),'--anim-track',mode],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=native('staged-'+mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r}; original={original!r}')
    print(f'PASS exact staged animated texture {mode}: input, time/endpoint states, canonical IDs and nested output')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
