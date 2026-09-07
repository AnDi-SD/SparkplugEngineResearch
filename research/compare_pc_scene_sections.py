#!/usr/bin/env python3
"""One bounded original scene-section/edge case vs compiled common source."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_scene_serializers import main as scalar
from probe_pc_render_node_sections import main as graph

def main(kind,mode):
    if kind=='graph' and mode not in {'inline','repeat','prebound'}:raise ValueError('only completed graph comparisons')
    if mode=='null-target':raise ValueError('host reference API has no null object target')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSceneSerializationTests.exe'
    args=['--graph',mode] if kind=='graph' else ['--capture',kind,mode]
    process=subprocess.run([str(binary),*args],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(process.stdout);native=graph(mode,True) if kind=='graph' else scalar(kind,mode,True)
    if source!=native:raise AssertionError(f'{kind}/{mode}: source={source!r}; native={native!r}')
    print(f'PASS exact {kind}/{mode}: identical input, result/cursor/scalars/edges and all writer bytes')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
