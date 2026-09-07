#!/usr/bin/env python3
"""Exact valid/empty-failure light codec comparison; native unsafe EOF is separate."""
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_light_serializer import main as native
def main(kind,mode):
 if mode in ('no-terminator','fail-write'):raise ValueError('native-only boundary case')
 original=native(kind,mode,True)
 result=subprocess.run([str(ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugLightSerializationTests.exe'),'--case',kind,mode,original[2]],capture_output=True,text=True,timeout=10,check=True)
 source=json.loads(result.stdout)
 if original!=source:raise AssertionError(f'{kind}/{mode}: source={source!r};original={original!r}')
 print(f'PASS exact Light serializer {kind}/{mode}: complete input, read/cursor, raw state, writer and fresh read')
 return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2],sys.argv[3]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
