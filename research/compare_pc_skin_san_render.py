#!/usr/bin/env python3
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_skin_san_render import main as original
from inspect_pc_san_keys import DEFAULT

def main(mode):
 native=original(mode,True);binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSkinRenderTests.exe'
 process=subprocess.run([str(binary),'--animated',str(DEFAULT/'bbush.san'),mode],input=native[0]+'\n'+native[1]+'\n',
  capture_output=True,text=True,timeout=10)
 if process.returncode:raise AssertionError(process.stderr)
 source=json.loads(process.stdout)
 if source!=native[2]:
  output=ROOT/'.codex-tmp/skin-san-comparison.json';output.write_text(json.dumps(dict(native=native[2],source=source),indent=2))
  raise AssertionError(f'{mode}: raw native/source mismatch saved to {output}')
 print('PASS exact original/source retained SAN actor/Skin palette/render',mode);return 0

if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
