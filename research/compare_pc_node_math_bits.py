#!/usr/bin/env python3
"""One bounded native/source batch for finite PC420350/420C00 rounding.

Both original routines run without seams. This sample does not claim universal
binary64 equivalence to x87 extended precision, NaN handling or all FP modes.
"""
from pathlib import Path
import hashlib
import json
import random
import struct
import subprocess
import sys
import time
from pc_instruction_emulator import PcInstructions, ROOT, run_bounded


def words(values):
    return list(struct.unpack('<'+'I'*len(values), struct.pack('<'+'f'*len(values), *values)))


def cases():
    ones=[1.0]*9
    yield 'vector-cancellation','v',[1e8,1.0,-1e8]+ones
    yield 'matrix-cancellation','m',[1e8,1.0,-1e8]*3+ones
    yield 'signed-zero-vector','v',[0.0,-0.0,0.0]+[-0.0]*9
    yield 'identity-matrix','m',[1,0,0,0,1,0,0,0,1]*2
    rng=random.Random(0x420C00)
    for i in range(96):
        for kind,n in [('v',12),('m',18)]:
            # Normal binary32 values around scene-transform magnitudes.
            bits=[(rng.randrange(2)<<31)|(rng.randrange(117,138)<<23)|rng.randrange(1<<23) for _ in range(n)]
            yield f'seeded-{kind}-{i}',kind,list(struct.unpack('<'+'f'*n,struct.pack('<'+'I'*n,*bits)))


def main():
    started=time.monotonic();batch=list(cases())
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugNodeWorldTests.exe'
    inputs=''.join(kind+' '+' '.join(map(str,words(values)))+'\n' for _,kind,values in batch)
    source=subprocess.run([str(binary),'--math-bits'],input=inputs,text=True,capture_output=True,timeout=10)
    assert source.returncode==0,source.stderr
    expected=[json.loads(line) for line in source.stdout.splitlines()]
    assert len(expected)==len(batch)
    p=PcInstructions();a=p.allocate(36);b=p.allocate(36);out=p.allocate(36)
    results=[];instructions=0
    for (name,kind,values),want in zip(batch,expected):
        split=3 if kind=='v' else 9
        p.put_floats(a,values[:split]);p.put_floats(b,values[split:])
        if kind=='v':p.run(0x420350,args=(out,a,b),callee_pop=False)
        else:p.run(0x420C00,this=a,args=(out,b))
        count=3 if kind=='v' else 9
        observed=list(struct.unpack('<'+'I'*count,p.mu.mem_read(out,4*count)))
        assert observed==want,(name,observed,want)
        instructions+=sum(p.visits.values());results.append([name,observed])
    assert results[0][1]==[0x3f800000]*3 and results[1][1]==[0x3f800000]*9
    report={'status':'passed','kind':'node-math-bit-comparison','cases':len(batch),
        'exactFloatWords':sum(len(row[1]) for row in results),'nativeInstructions':instructions,
        'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),
        'executionProfile':'micro','arenaReservedBytes':p.allocated,'seams':len(p.seams),
        'seconds':time.monotonic()-started,'results':results}
    path=ROOT/'local-data/results/cycle-20260908-0700/cp107-node-math-bits.json'
    path.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='results'},sort_keys=True))
    return 0


if __name__=='__main__':
    raise SystemExit(main() if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
