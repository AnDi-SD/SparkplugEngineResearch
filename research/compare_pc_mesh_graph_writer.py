#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_mesh_writer import main as original

def main(mode,policy,kind):
 native=original(mode,policy,True,kind=='base',True)
 result=subprocess.run([str(ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMeshReaderTests.exe'),'--write-graph',mode,policy,kind],input='\n'.join(native[2:4])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=native:raise AssertionError((native,source))
 print(f'PASS exact generic mesh index/write/repeat/null and FAT metadata {mode}/{policy}/{kind}');return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
