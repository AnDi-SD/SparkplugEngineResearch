#!/usr/bin/env python3
"""Both original and compiled paths read the unchanged complete bbush FFPS."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from inspect_pc_san_keys import DEFAULT
from probe_pc_san_loader import load_asset
from compare_pc_san_reader import compare

def main():
    portable=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSanReaderTests.exe'
    if '--portable-corpus' in sys.argv:
        # Independent already-tested field reader is a composed regression,
        # NOT whole-original execution of the three capped cold loaders.
        tracks=0
        for name in ('bbush.san','bflower.san','barrel.san','bw.san'):
            results=[]
            for mode in ('--inspect-file','--inspect-owned'):
                output=subprocess.run([str(portable),mode,str(DEFAULT/name)],
                    cwd=ROOT,capture_output=True,text=True,timeout=10,check=True).stdout
                if len(output)>150000:raise ValueError('bounded output exceeded')
                result=json.loads(output);del result['usedPools'];results.append(result)
            compare(results[1],results[0])
            tracks+=len(results[0]['tracks'])
        print(f'PASS 4/4 files: portable full-file/owned-field equality; {tracks} tracks; not whole-original corpus evidence')
        return 0
    output=subprocess.run([str(portable),'--inspect-file',str(DEFAULT/'bbush.san')],
        cwd=ROOT,capture_output=True,text=True,timeout=10,check=True).stdout
    if len(output)>150000:raise ValueError('bounded output exceeded')
    actual=json.loads(output);expected=load_asset('bbush.san',quiet=True)
    del actual['usedPools'];del expected['usedPools']
    count=compare(expected,actual)
    print(f'PASS {count}/{count}: original/portable WHOLE FFPS load, owned names and PRS')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
