#!/usr/bin/env python3
"""Exact original engine/reconstructed callbacks on complete RFX event streams."""
from pathlib import Path
import hashlib,json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_rfx_corpus import main as original,events_for,FILES
from compare_pc_rfx_events import event_input

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugEffectTemplateTests.exe'
    native=original(mode,True)
    source=json.loads(subprocess.run([str(binary),'--corpus',mode],input=event_input(events_for(mode)),capture_output=True,text=True,timeout=10,check=True).stdout)
    if source!=native:
        for index,(a,b) in enumerate(zip(source,native)):
            if a!=b:raise AssertionError(f'{mode}: differing top-level field{index}; native/source resource data intentionally not printed')
        raise AssertionError('capture shape mismatch')
    fingerprint=hashlib.sha256(json.dumps(native,ensure_ascii=True,separators=(',',':')).encode()).hexdigest()
    print('PASS exact PC RFX corpus',mode,'documentSha256='+FILES[mode][1],'captureSha256='+fingerprint);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
