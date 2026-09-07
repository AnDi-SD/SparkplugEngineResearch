#!/usr/bin/env python3
"""Native actual DX texture branch vs resolved binding consumer slice."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_binding import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMaterialApplyTests.exe'
    source=json.loads(subprocess.run([str(binary),'--binding',mode],capture_output=True,text=True,timeout=10,check=True).stdout)
    original=native(mode,True)
    if source!=original:raise AssertionError(f'binding {mode}: source={source!r}; native={original!r}')
    print(f'PASS exact PC texture binding {mode}: cache/COM portion, not alternate RTTI resources or GPU');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
