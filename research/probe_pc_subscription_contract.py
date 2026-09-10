#!/usr/bin/env python3
"""Original subscription map lifecycle, ordering and nonmutating dispatch.

Only allocation/SEH and receiver notification observation are synthetic.
Callbacks do not change subscription state; mutation is outside this probe.
"""
from pathlib import Path
import json,hashlib,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter();f=LifetimeFixture();p=f.p
    report=dict(kind='original-pc-subscription-contract',status='running',stages=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        executableSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        scope='Original manager factory,add/remove/count/dispatch/destructor; explicit borrowed notification receivers, nonmutating callback observations only',
        limits='micro100000 instructions/2s each call;30s child; allocator and nonthrowing SEH only; no host DLL/game startup')
    calls=[];base=0x34150000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def callback(m):
        msg=m.uint(m.reg('ESP')+4);calls.append(dict(receiver=m.reg('ECX'),notification=msg,key=m.uint(msg+0xc)));m.fixture_return(4)
    p.seams[base]=callback;vt=p.allocate(8);p.put_uint(vt+4,base)
    receivers=[p.allocate(0x10) for _ in range(3)];message=p.allocate(0x20);p.mu.mem_write(message,bytes(0x20))
    for a in receivers:p.mu.mem_write(a,bytes(0x10));p.put_uint(a,vt)
    def save():
        report.update(seconds=time.perf_counter()-started,lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{x:08X}' for x in p.tail],
            allocations=[dict(address=a,bytes=n,freed=a in f.freed) for a,n in f.allocations.items()])
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        manager=f.call(0x4165b0);report.update(manager=manager,receivers=receivers,initialBytes=bytes(p.mu.mem_read(manager,0x20)).hex());save()
        def snapshot(label,key=42):
            count=f.call(0x415990,manager,(key,));p.put_uint(message+0xc,key);calls.clear();f.call(0x415a20,manager,(message,))
            row=dict(label=label,key=key,groupCount=p.uint(manager+0x1c),subscriberCount=count,
                callbacks=list(calls),managerBytes=bytes(p.mu.mem_read(manager,0x20)).hex(),
                outstanding=[a for a in f.allocations if a not in f.freed])
            report['stages'].append(row);save();return row
        def expect(label,expected,key=42):
            row=snapshot(label,key);assert row['subscriberCount']==len(expected)
            assert [x['receiver'] for x in row['callbacks']]==expected,(label,row,expected)
        expect('empty',[])
        # Reverse arrival order discriminates address order from insertion order.
        for a in reversed(receivers):f.call(0x416150,manager,(42,a))
        expect('three-reverse-inserted',receivers)
        f.call(0x416150,manager,(42,receivers[1]));expect('duplicate-suppressed',receivers)
        f.call(0x416150,manager,(99,receivers[2]));expect('independent-group',[receivers[2]],99)
        expect('unknown-group',[],77)
        f.call(0x4163a0,manager,(42,receivers[1]));expect('remove-middle',[receivers[0],receivers[2]])
        f.call(0x416150,manager,(42,receivers[1]));expect('reinsert-middle',receivers)
        f.call(0x4163a0,manager,(42,receivers[1]));f.call(0x4163a0,manager,(42,receivers[1]));expect('missing-member-no-change',[receivers[0],receivers[2]])
        for a in (receivers[0],receivers[2]):f.call(0x4163a0,manager,(42,a))
        expect('empty-group-erased',[]);assert p.uint(manager+0x1c)==1
        f.call(0x4163a0,manager,(99,receivers[2]));expect('all-groups-erased',[]);assert p.uint(manager+0x1c)==0
        left=[(a,n) for a,n in f.allocations.items() if a not in f.freed]
        report['retainedEmptyManager']=dict(allocations=left,bytes={f'{a:08X}':bytes(p.mu.mem_read(a,n)).hex() for a,n in left})
        assert len(left)==2 and all(n==0x20 for a,n in left);save()
        f.call(p.uint(p.uint(manager)),manager,(1,));assert set(f.allocations)==set(f.freed)
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error));save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',stages=len(report['stages']),seconds=report['seconds'],allocations=len(f.allocations),freed=len(f.freed))));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(args[1]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
