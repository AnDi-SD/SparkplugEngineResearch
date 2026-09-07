#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_mesh_writer import main as original

def main(mode,policy):
 native=original(mode,policy,True,True)
 binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMeshReaderTests.exe'
 result=subprocess.run([str(binary),'--write-base',mode,policy],input='\n'.join(native[2:4])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=native:raise AssertionError((native,source))
 print(f'PASS exact whole original/source base mesh writer {mode}/{policy}');return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
