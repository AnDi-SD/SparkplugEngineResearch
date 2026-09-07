#!/usr/bin/env python3
"""Exact original material UV/PC renderer cache/device inputs vs source."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_uv_renderer import main as native

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugUVFunctionTests.exe'
    source=json.loads(subprocess.run([str(binary),'--uv-renderer',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r}; original={original!r}')
    print(f'PASS exact UV renderer {mode}: holder matrices, caches, submission order/bytes and recompute count')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
