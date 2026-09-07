#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_skin_material_mesh_render import main as original,MODES
from inspect_pc_san_keys import DEFAULT
def main(mode):
 native=original(mode,True);delta,device_mode=MODES[mode]
 binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSkinRenderTests.exe'
 result=subprocess.run([str(binary),'--material-mesh-generated',str(DEFAULT/'bbush.san'),delta,device_mode],
  input='\n'.join(native[:4])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=native[4]:
  path=ROOT/'.codex-tmp/skin-material-mesh-comparison.json';path.write_text(json.dumps(dict(native=native[4],source=source),indent=2))
  raise AssertionError(f'{mode}: raw mismatch saved to {path}')
 print('PASS exact original/source material SAN scene mesh Skin draw',mode);return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
