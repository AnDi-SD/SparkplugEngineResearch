#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from inspect_pc_san_keys import DEFAULT
from probe_pc_actor_control_pipeline import main as original

def main(mode):
 native=original(mode,True);binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugActorBindingTests.exe'
 result=subprocess.run([str(binary),'--control-pipeline',str(DEFAULT/'bbush.san'),mode],capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=native:raise AssertionError(f'{mode}: native={native!r}; source={source!r}')
 print('PASS exact original/source actor capacity/control/tick pipeline',mode);return 0

if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
