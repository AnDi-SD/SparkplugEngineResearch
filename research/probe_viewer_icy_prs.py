"""Bounded original-PC PRS for one actual Viewer discrepancy; no corpus scan."""
from pathlib import Path
import hashlib
import json
import sys
import time
from inspect_pc_san_keys import inspect
from pc_instruction_emulator import PcInstructions, run_bounded, ROOT
from probe_pc_animation_keys import install_acos_seams
from probe_pc_san_vmd_keys import NativeTrack

def guest():
    started=time.perf_counter()
    path=ROOT/'local-data/pc-pristine/Media/Characters/Icy/xiwa.san'
    summary,tracks=inspect(path)
    machine=PcInstructions()
    install_acos_seams(machine)
    rows=[]
    times=[0.0,0.033333335,0.5,1.0] if '--all-times' in sys.argv else [1.0]
    for index,track in enumerate(tracks):
        native=NativeTrack(machine,{role:value['payload'] for role,value in track['roles'].items()})
        for seconds in times:
            prs,flags=native.sample(seconds)
            rows.append({'ordinal':index,'name':track['name'],'seconds':seconds,'prs':prs,'validity':list(flags)})
    output=ROOT/'local-data/results/viewer-sparkplug-core-20260908'/('icy-original-prs-all.json' if '--all-times' in sys.argv else 'icy-original-prs.json')
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps({'source':summary,'exeSha256':hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest(),
        'rows':rows,'elapsedSeconds':time.perf_counter()-started,'scope':'Original reader43DB90 and sampler479290; one actual clip. Existing CRT acos/stream/allocator seams; 64KiB guest,100k instructions/2s per call,30s outer.'},indent=2),encoding='utf-8')
    print(f'Captured {len(rows)} original Icy tracks in {time.perf_counter()-started:.3f}s')

if __name__=='__main__':
    if '--guest' in sys.argv: guest()
    else: raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
