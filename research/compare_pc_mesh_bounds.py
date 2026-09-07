#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_mesh_bounds import main as original

def main(mode):
 native=original(mode,True);binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSkinRenderTests.exe'
 result=subprocess.run([str(binary),'--mesh-bounds',mode],input='\n'.join(native[:2])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=native[2]:raise AssertionError((mode,native[2],source))
 print('PASS exact original/source sphere and primitive-count uint16 AABB',mode);return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
