#!/usr/bin/env python3
from pathlib import Path
import sys,json,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_light_scene_world import main as native
def main(mode):
 original=native(mode,True)
 result=subprocess.run([str(ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugLightSerializationTests.exe'),'--scene-world',mode,original[1]],capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 if source!=original:
  path=ROOT/'.codex-tmp/light-scene-world-comparison.json';path.write_text(json.dumps(dict(native=original,source=source),indent=2),encoding='utf-8');raise AssertionError(f'{mode}: mismatch saved to {path}')
 print(f'PASS exact decoded light actual Scene/World/LightManager targets {mode}');return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
