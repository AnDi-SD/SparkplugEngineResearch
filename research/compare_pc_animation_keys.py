#!/usr/bin/env python3
"""Compare C++ reconstruction output to original PC preparation/sampling code."""
from pathlib import Path
import argparse
import json
import subprocess
from pc_instruction_emulator import ROOT,PcInstructions,run_bounded
from probe_pc_animation_keys import setup_track,install_acos_seams,near


def guest(binary):
    binary=binary.resolve()
    if binary.name!='SparkplugAnimationKeyTests.exe' or not binary.is_relative_to(ROOT):
        raise ValueError('Only the workspace reconstruction test binary is permitted')
    p=PcInstructions();install_acos_seams(p)
    result=subprocess.run([str(binary),'--emit-fixtures'],check=True,capture_output=True,text=True,timeout=10)
    if len(result.stdout)>100_000:raise ValueError('Unexpected fixture output size')
    rows=[json.loads(line) for line in result.stdout.splitlines()]
    if len(rows)!=40:raise ValueError('Expected 40 deterministic fixtures')
    previous=0;checks=0
    for row in rows:
        rep,time=row[:2]
        if rep!=previous:
            p.reset_arena()
            track,cache,sample,_=setup_track(p,rep)
            for role in range(3):
                for axis in range(3 if rep>=3 else 1):
                    descriptor=p.uint(track+0x18+role*12+axis*4)
                    if rep==2 or rep==4:
                        entry=0x493160 if rep==4 else 0x4933c0 if role==1 else 0x493290
                        p.run(entry,0,[p.uint(descriptor+12),p.uint(descriptor)],callee_pop=False)
            previous=rep
        actual=sample(time)
        flattened=(*actual[0],*actual[1],*actual[2])
        if not near(flattened,row[2:12]):
            raise AssertionError(f'PC/C++ mismatch rep={rep}, time={time}: {flattened} != {row[2:12]}')
        if [p.uint(cache+4*i) for i in range(9)]!=row[12:21]:
            raise AssertionError('PC/C++ key-cache mismatch')
        if actual[3]!=b'\1\1\1':raise AssertionError('Native validity mismatch')
        checks+=3
    print(f'PASS {checks}/{checks}: 40 C++/original-PC PRS, cache and validity comparisons')
    return 0


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--guest',action='store_true')
    parser.add_argument('--portable',type=Path,required=True)
    args=parser.parse_args()
    raise SystemExit(guest(args.portable) if args.guest else
                     run_bounded(Path(__file__),['--portable',str(args.portable)]))
