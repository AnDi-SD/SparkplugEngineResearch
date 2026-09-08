#!/usr/bin/env python3
"""Serial fresh-child timing of identical original/source checks with rotated mode order."""
from pathlib import Path
import hashlib,json,statistics,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]

def main():
    directory=ROOT/'local-data/results/cycle-20260908-0700';rows=[];semantic={}
    for repeat in range(3):
        for tag in (1,7):
            for mode in (('single','batch') if repeat%2==0 else ('batch','single')):
                command=[sys.executable,str(ROOT/'research/compare_pc_particle_sampling.py'),'--guest',str(tag)]+(['--source-single'] if mode=='single' else [])
                started=time.monotonic();result=subprocess.run(command,capture_output=True,timeout=30)
                elapsed=time.monotonic()-started
                if result.returncode:raise RuntimeError((result.stdout+result.stderr).decode(errors='replace')[-4000:])
                suffix='-single' if mode=='single' else '';report=json.loads((directory/f'cp121-sampling-{tag}{suffix}.json').read_text())
                digest=hashlib.sha256(json.dumps(report['cases'],sort_keys=True).encode()).hexdigest().upper()
                if tag in semantic:assert semantic[tag]==digest,'positions, RNG state, instructions and call counts stay exact'
                semantic[tag]=digest;row=dict(repeat=repeat,tag=tag,sourceMode=mode,processSeconds=elapsed,semanticSha256=digest,
                    sourceProcessCount=report['sourceProcessCount'],sourceExecutableSha256=report['sourceExecutableSha256'])
                rows.append(row);print(json.dumps(row),flush=True)
    comparisons=[]
    for tag in (1,7):
        medians={mode:statistics.median(r['processSeconds'] for r in rows if r['tag']==tag and r['sourceMode']==mode) for mode in ('single','batch')}
        comparisons.append(dict(tag=tag,medians=medians,speedup=medians['single']/medians['batch']))
    report=dict(kind='native-particle-source-batch-benchmark',rows=rows,comparisons=comparisons)
    (directory/'cp122-particle-batch-benchmark.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(comparisons),flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
