#!/usr/bin/env python3
"""Original PC FVF/element emitter vs reconstruction; guest-only, hard caps."""
import json
from pathlib import Path
import subprocess
import sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture

def cases():
    values=[0,0xffffffff,0x840,0x940,0x20,0x93e,0x1fffff,6,0xe,0x1e,0x3e]
    values.extend(1<<i for i in range(32))
    state=0x534d4f
    for _ in range(64):
        state=(state*1664525+1013904223)&0xffffffff
        values.append(state&0x1fffff)
    return list(dict.fromkeys(values))

def main():
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugVertexDeclarationTests.exe'
    result=subprocess.run([str(binary),'--capture'],capture_output=True,text=True,timeout=10,check=True)
    rows=json.loads(result.stdout)
    if [r[0] for r in rows]!=cases():raise AssertionError('deterministic fixture masks differ')
    f=LifetimeFixture();p=f.p;obj=f.call(0x4c9c20);compared=0;max_instructions=0
    for flags,fvf,allocation_bytes,wanted_hex in rows:
        f.call(0x4b23c0,this=obj,args=(flags,))
        if p.uint(obj+0x14)!=fvf:raise AssertionError(f'{flags:#x}: base FVF differs')
        p.run(0x4c9a00,args=(flags,),callee_pop=False)
        max_instructions=max(max_instructions,sum(p.visits.values()))
        array=p.reg('EAX')
        if array not in f.allocations:raise AssertionError('emitter did not allocate')
        size=f.allocations[array]
        if size!=allocation_bytes or size>256:raise AssertionError(f'{flags:#x}: allocation differs')
        raw=bytes(p.mu.mem_read(array,size));terminal=bytes.fromhex('ff00000011000000')
        ends=[i+8 for i in range(0,size,8) if raw[i:i+8]==terminal]
        if len(ends)!=1:raise AssertionError(f'{flags:#x}: terminal not unique')
        end=ends[0]
        if raw[end:]!=b'\xcc'*(size-end):raise AssertionError('unexpected initialized tail')
        if raw[:end].hex()!=wanted_hex:
            raise AssertionError(f'{flags:#x}: native={raw[:end].hex()} source={wanted_hex}')
        compared+=end
        p.run(0x412420,args=(array,),callee_pop=False)
    f.call(0x4c9ce0,this=obj,args=(1,))
    if set(f.allocations)!=set(f.freed):raise AssertionError('tracked allocation leak')
    print(f'PASS {len(rows)}/{len(rows)} PC declaration masks; {compared} exact used bytes; '
          f'FVF, allocation capacity, elements and untouched tail; max emitter instructions={max_instructions}; '
          f'heap={p.allocated}/65536; no graphics device')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
