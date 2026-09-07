#!/usr/bin/env python3
"""Actual PC CPU texture section/state/pixel/output vs compiled common codec."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_cross import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    result=subprocess.run([str(binary),'--cross',mode],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r};original={original!r}')
    print(f'PASS exact PC CPU texture {mode}: sections/state/opaque pixels/output')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
