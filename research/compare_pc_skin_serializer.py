#!/usr/bin/env python3
"""Compare actual Skin graph semantics and entire native/source wire output."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_skin_serializer import main as original,specimen

def main(mode):
    native=original(mode,True);directory,payload,_=specimen(mode)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSkinSerializationTests.exe'
    source=json.loads(subprocess.run([str(binary),'--capture',mode],input=directory.hex()+'\n'+payload.hex()+'\n',
        capture_output=True,text=True,timeout=10,check=True).stdout)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact original/source Skin graph',mode);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
