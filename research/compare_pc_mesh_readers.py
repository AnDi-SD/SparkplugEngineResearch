#!/usr/bin/env python3
"""One bounded native mesh-reader case vs compiled reconstruction per child."""
import json,subprocess,sys
from pathlib import Path
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_mesh_serializer import main as native

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMeshReaderTests.exe'
    result=subprocess.run([str(binary),'--capture'],capture_output=True,text=True,timeout=10,check=True)
    rows=json.loads(result.stdout);source=next(row for row in rows if row[0]==mode)
    original=native(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r} original={original!r}')
    print(f'PASS10/10 exact PC mesh-reader fields: {mode}; identical input, cursor, FVF, stride, sizes, component count and vertex/index bytes')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
