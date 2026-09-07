#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_skin_alpha_flush import main as original,MODES

def main(mode):
 native=original(mode,True);binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSkinRenderTests.exe'
 result=subprocess.run([str(binary),'--alpha-flush',mode],input='\n'.join(native[:2])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=native[2]:
  path=ROOT/'.codex-tmp/skin-alpha-flush-comparison.json';path.write_text(json.dumps(dict(native=native[2],source=source),indent=2))
  raise AssertionError(f'{mode}: raw mismatch saved to {path}')
 print('PASS exact original/source whole owning Skin alpha flush',mode);return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
