#!/usr/bin/env python3
"""Fresh-process whole-operation cache benchmark; all native guards retained."""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import statistics
import subprocess
import sys
import time
from unittest.mock import patch
from pc_instruction_emulator import ROOT, PcInstructions

DIRECTORY = ROOT/'local-data/results/cycle-20260908-0700'
MODES = ('baseline', 'fast-pe', 'page')
CASES = ('g-crystal', 'gem-animated', 'p8')


def peak_working_set():
    if sys.platform != 'win32': return None
    class Counters(ctypes.Structure):
        _fields_ = [('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[
            (name,ctypes.c_size_t) for name in ('peak','working','peakPaged','paged',
                'peakNonpaged','nonpaged','pagefile','peakPagefile')]
    value = Counters();value.cb = ctypes.sizeof(value)
    get_process = ctypes.windll.kernel32.GetCurrentProcess
    get_process.restype = ctypes.c_void_p
    get_memory = ctypes.windll.psapi.GetProcessMemoryInfo
    get_memory.argtypes = [ctypes.c_void_p,ctypes.POINTER(Counters),ctypes.c_ulong]
    if not get_memory(get_process(),ctypes.byref(value),value.cb):
        raise ctypes.WinError()
    return value.peak


def guest(mode, case, iteration):
    assert mode in MODES and case in CASES and 0 <= iteration < 9
    sys.path[:0] = [str(ROOT/'local-data/research-cache/python')]
    import pefile
    initialize, parse = PcInstructions.__init__, pefile.PE.__init__
    instances = []
    def make(instance, *args, **kwargs):
        kwargs['code_cache_mode'] = 'page' if mode == 'page' else 'bytes'
        initialize(instance, *args, **kwargs);instances.append(instance)
    def image(instance, *args, **kwargs):
        kwargs['fast_load'] = mode != 'baseline'
        return parse(instance, *args, **kwargs)
    at = time.monotonic()
    with patch.object(PcInstructions,'__init__',make), patch.object(pefile.PE,'__init__',image):
        if case == 'p8':
            from compare_pc_texture_cross_upload import main
            label = f'cache-{mode}-{iteration}'
            assert main(2,'3x5','random',label,'dx') == 0
            path = DIRECTORY/f'cp115-cross-comparison-2-3x5-random-{label}.json'
            native = json.loads(path.read_text())
            semantic = native['observed']
            instructions = native['nativeInstructions']
        else:
            from probe_pc_scene_file_profile import main
            assert main(case,True) == 0
            stem = 'gem-animated' if case == 'gem-animated' else 'g-crystal-ids'
            native = json.loads((DIRECTORY/f'cp109-{stem}.json').read_text())
            semantic = json.loads((DIRECTORY/f'cp109-{stem}-native.json').read_text())
            instructions = native['wholeLoadInstructions']
    elapsed = time.monotonic()-at
    assert len(instances) == 1 and native['status'] == 'passed'
    report = dict(kind='native-instruction-cache-benchmark',mode=mode,case=case,
        iteration=iteration,status='passed',elapsedSeconds=elapsed,
        peakWorkingSetBytes=peak_working_set(),nativeInstructions=instructions,
        releasedAllocations=native['releasedAllocations'],arenaReservedBytes=native['arenaReservedBytes'],
        semanticSha256=hashlib.sha256(json.dumps(semantic,sort_keys=True).encode()).hexdigest().upper(),
        cacheEntries=len(instances[0].code_cache),cachePages=len(instances[0].code_cache_pages),
        nativeComparison={key:value for key,value in native.items() if key not in ('observed','expected')})
    (DIRECTORY/f'cp117-cache-{mode}-{case}-{iteration}.json').write_text(json.dumps(report,indent=2)+'\n')
    print('BENCHMARK',json.dumps({k:v for k,v in report.items() if k!='nativeComparison'}),flush=True)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--guest',nargs=3,metavar=('MODE','CASE','ITERATION'))
    parser.add_argument('--repeats',type=int,choices=range(1,10),default=3)
    args = parser.parse_args()
    if args.guest: return guest(args.guest[0],args.guest[1],int(args.guest[2]))
    rows = [];at = time.monotonic()
    for iteration in range(args.repeats):
        for case in CASES:
            # Rotate order to reduce consistent first/last-run advantage.
            for mode in MODES[iteration%3:]+MODES[:iteration%3]:
                log = ROOT/f'.codex-tmp/cp117-cache-{mode}-{case}-{iteration}.log'
                child_at = time.monotonic()
                with log.open('wb') as output:
                    result = subprocess.run([sys.executable,str(Path(__file__).resolve()),
                        '--guest',mode,case,str(iteration)],cwd=ROOT,stdout=output,
                        stderr=subprocess.STDOUT,timeout=30)
                if result.returncode:
                    print(log.read_text(errors='replace')[-3000:]);return result.returncode
                row = json.loads((DIRECTORY/f'cp117-cache-{mode}-{case}-{iteration}.json').read_text())
                row['processSeconds'] = time.monotonic()-child_at
                rows.append(row)
                print(case,mode,iteration,round(row['processSeconds'],3),flush=True)
    summary = {}
    for case in CASES:
        selected = [row for row in rows if row['case']==case]
        assert len({row['semanticSha256'] for row in selected}) == 1
        assert len({row['nativeInstructions'] for row in selected}) == 1
        medians = {mode:statistics.median(row['processSeconds'] for row in selected if row['mode']==mode) for mode in MODES}
        summary[case] = dict(medianProcessSeconds=medians,speedup=medians['baseline']/medians['page'])
    report = dict(status='passed',repeats=args.repeats,elapsedSeconds=time.monotonic()-at,
        cases=rows,summary=summary,processTimeoutSeconds=30,workers=1)
    (DIRECTORY/'cp117-cache-benchmark.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(summary),flush=True);return 0


if __name__=='__main__': raise SystemExit(main())
