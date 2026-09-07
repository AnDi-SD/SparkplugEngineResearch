#!/usr/bin/env python3
"""One original Fog successful section vs compiled source; raw input/bits/output."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_fog_serializer import main as native
from probe_pc_fog_links import main as native_graph

def main(mode,kind='scalar'):
    allowed={'empty','values','repeat','raw-bits','logo-field'} if kind=='scalar' else {'inline','repeat','clear','prebound','named'}
    if kind not in {'scalar','graph'} or mode not in allowed:raise ValueError('explicit successful Fog differential; unsafe native lifetime is not reproduced')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSceneSerializationTests.exe'
    result=subprocess.run([str(binary),'--fog' if kind=='scalar' else '--fog-graph',mode],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=(native if kind=='scalar' else native_graph)(mode,True)
    if source!=original:raise AssertionError(f'{mode}: source={source!r};original={original!r}')
    print(f'PASS exact Fog {mode}: identical input, return/cursor,20state bytes and complete output')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
