#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_skin_queued_generated_render import main as original
from inspect_pc_san_keys import DEFAULT

def main(mode):
 native=original(mode,True);binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSkinRenderTests.exe'
 result=subprocess.run([str(binary),'--queued-generated',str(DEFAULT/'bbush.san'),mode],input='\n'.join(native[:3])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=native[3]:
  path=ROOT/'.codex-tmp/skin-queued-generated-comparison.json';path.write_text(json.dumps(dict(native=native[3],source=source),indent=2))
  raise AssertionError(f'{mode}: raw mismatch saved to {path}')
 print('PASS exact original/source whole queued generated SAN scene mesh Skin draw',mode);return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
