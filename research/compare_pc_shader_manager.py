#!/usr/bin/env python3
"""Exact original/source PC manager cache or key; no compiler/GPU claim."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_shader_manager import main as manager
from probe_pc_shader_key import main as key
def main(family,mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugShaderManagerTests.exe'
    if family not in ('manager','key'):raise ValueError('explicit bounded family')
    source=json.loads(subprocess.run([str(binary),'--case' if family=='manager' else '--key',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=(manager if family=='manager' else key)(mode,True)
    if source!=original:raise AssertionError(f'{family}/{mode}: source={source!r}; native={original!r}')
    print(f'PASS exact PC shader {family}/{mode}; miss generation remains incomplete');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
