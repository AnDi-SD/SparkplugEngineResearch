#!/usr/bin/env python3
"""One original Node reader/writer/graph case vs compiled source per child."""
import json,subprocess,sys
from pathlib import Path
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_node_serializer import main as reader
from probe_pc_node_writer import main as writer
from probe_pc_node_save_graph import main as graph

def main(kind,mode=None):
    if kind not in {'reader','writer','graph'}:raise ValueError('explicit Node comparison category')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugNodeSerializationTests.exe'
    result=subprocess.run([str(binary),'--capture'],capture_output=True,text=True,timeout=10,check=True)
    rows=json.loads(result.stdout)
    source=rows['graph'] if kind=='graph' else next(r for r in rows[kind+'s'] if r[0]==mode)
    original=graph(True) if kind=='graph' else (reader if kind=='reader' else writer)(mode,True)
    if original!=source:raise AssertionError(f'{kind}/{mode}: native={original!r} source={source!r}')
    print(f'PASS exact original/source Node {kind}/{mode}: input/float bits/flags/cursor or complete output bytes')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
