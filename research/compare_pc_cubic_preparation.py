#!/usr/bin/env python3
"""Bit-exact scalar/vector cubic setup; 96 cases, preserved native spill sites."""
import json
from pathlib import Path
import struct
import subprocess
import sys
from pc_instruction_emulator import PcInstructions,ROOT,run_bounded


def main():
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugAnimationKeyTests.exe'
    result=subprocess.run([str(binary),'--emit-cubic-bits'],capture_output=True,text=True,timeout=10,check=True)
    rows=[json.loads(line) for line in result.stdout.splitlines()]
    if len(rows)!=96:raise AssertionError('expected96 deterministic cubic fixtures')
    p=PcInstructions();compared=0
    for index,(dimensions,count,source,wanted) in enumerate(rows):
        if dimensions not in (1,3) or count not in (1,2,3,5,6,9):raise AssertionError('invalid fixture')
        if len(source)!=count*dimensions*5 or len(wanted)!=len(source):raise AssertionError('fixture extent')
        p.reset_arena();address=p.allocate(len(source)*4)
        p.mu.mem_write(address,struct.pack('<'+'I'*len(source),*source))
        p.run(0x493160 if dimensions==1 else 0x493290,args=(address,count),callee_pop=False)
        actual=list(struct.unpack('<'+'I'*len(source),p.mu.mem_read(address,len(source)*4)))
        if actual!=wanted:
            mismatch=[(i,hex(a),hex(b)) for i,(a,b) in enumerate(zip(actual,wanted)) if a!=b]
            raise AssertionError(f'fixture{index} dim{dimensions} count{count}: {mismatch[:8]}')
        compared+=len(source)
    print(f'PASS 96/96 cubic preparation cases; {compared} bit-exact scalar values, all scalar/vector loop sizes')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
