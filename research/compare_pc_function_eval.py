#!/usr/bin/env python3
"""Native finite scalar/clock and codec vs compiled source; no algorithm seam."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_function_eval import main as native

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugFunctionEvalTests.exe'
    result=subprocess.run([str(binary),'--function',mode],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r}; original={original!r}')
    print(f'PASS exact FunctionEval {mode}: scalar/clock states, RNG values, input and common writer bytes')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
