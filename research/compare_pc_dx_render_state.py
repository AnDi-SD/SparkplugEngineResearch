#!/usr/bin/env python3
"""Original DX4B0A90 vs portable cache-entry semantics, explicit COM leaf."""
from pathlib import Path
import random
import subprocess
import sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_renderer_queues import QueueFixture


def main():
    f=QueueFixture()
    p=f.p
    device,table=p.allocate(4),p.allocate(0xe8)
    p.put_uint(device,table)
    p.put_uint(f.renderer+0xc9e8,device)
    p.put_uint(table+0xe4,f.RX+0x80)
    calls=[]
    hresult=0
    def state(p):
        receiver,index,value=(p.uint(p.reg('ESP')+4*i) for i in (1,2,3))
        assert receiver==device and index<256
        calls.append((index,value))
        p.fixture_return(12,eax=hresult)
    p.seams[f.RX+0x80]=state
    rng=random.Random(0x4b0a90)
    rows,expected=[],[]
    for case in range(96):
        index=rng.choice((0,1,52,53,174,210,255)) # fixture bounds, not original array extent claim
        old=rng.randrange(0x100000000)
        first=old if case%3==0 else rng.randrange(0x100000000)
        second=first if case%2==0 else rng.randrange(0x100000000)
        hresult=rng.choice((0,1,0xffffffff,0x88760868))
        rows.append(' '.join(map(str,(old,index,first,second,hresult))))
        p.put_uint(f.renderer+0xe4f4+index*4,old)
        calls.clear()
        observed=[]
        for value in (first,second):
            result=f.call(0x4b0a90,this=f.renderer,args=(index,value))&255
            observed.extend((result,p.uint(f.renderer+0xe4f4+index*4),len(calls),
                             *(calls[-1] if calls else (0,0))))
        expected.append(observed)
    exe=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugDXRenderStateTests.exe'
    result=subprocess.run([str(exe),'--batch'],input='\n'.join(rows)+'\n',
                          text=True,capture_output=True,timeout=20,check=True)
    actual=[list(map(int,line.split(','))) for line in result.stdout.splitlines()]
    assert len(actual)==len(expected)
    for i,(a,e) in enumerate(zip(actual,expected)):assert a==e,(i,a,e)
    f.close()
    print(f'PASS {sum(map(len,expected))}/{sum(map(len,expected))}: DX state cache/96 two-call cases')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
