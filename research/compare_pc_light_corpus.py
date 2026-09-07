#!/usr/bin/env python3
"""Exact original full compact SMO light reader/writer vs compiled source."""
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_light_corpus import main as native
def main(case):
 original=native(case,True)
 result=subprocess.run([str(ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugLightSerializationTests.exe'),'--case','data',case,original[2]],capture_output=True,text=True,timeout=10,check=True)
 source=json.loads(result.stdout)
 if source!=original:raise AssertionError(f'{case}: source={source!r};original={original!r}')
 print(f'PASS exact SMO light {case}: all light/Node state and complete output/fresh read')
 return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
