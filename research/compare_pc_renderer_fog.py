#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_renderer_fog import main as original,MODES
def main(mode):
 native=original(mode,True);binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugRendererFogTests.exe'
 result=subprocess.run([str(binary),'--case',mode],input='\n'.join(native[1])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=native:raise AssertionError(dict(mode=mode,native=native,source=source))
 print('PASS exact original/source decoded Fog renderer',mode);return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
