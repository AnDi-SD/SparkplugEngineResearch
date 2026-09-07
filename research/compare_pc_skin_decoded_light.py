#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_skin_decoded_light import main as native
from inspect_pc_san_keys import DEFAULT
def main(case,mode='normal'):
 original=native(case,mode,True)
 result=subprocess.run([str(ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSkinRenderTests.exe'),'--decoded-light',str(DEFAULT/'bbush.san'),case,mode],input='\n'.join(original[:4])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=original[4]:
  path=ROOT/'.codex-tmp/skin-decoded-light-comparison.json';path.write_text(json.dumps(dict(native=original[4],source=source),indent=2),encoding='utf-8');raise AssertionError(f'{case}/{mode}: mismatch saved to {path}')
 print(f'PASS exact same decoded SMO light to virtual world and whole SAN/Skin draw {case}/{mode}');return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
