#!/usr/bin/env python3
"""Original PathFinder lifecycle in an explicit empty scene graph."""
from pathlib import Path
import hashlib,json,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_task_timer import TimerFixture


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter();f=TimerFixture();p=f.p
    report=dict(kind='original-pc-pathfinder-empty-graph',status='running',stages=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        executableSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        scope='Actual Node factory/search and PathFinder lifecycle; borrowed engine+18->scene+14 fields. Empty graph only, no engine/scene startup or substituted PathFinder/search body.',
        limits='micro100000/2s per operation;30s child; existing allocator/SEH/clock/clone-pair seams')
    def save():
        report.update(seconds=time.perf_counter()-started,lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{a:08X}' for a in p.tail],allocations=[dict(address=a,bytes=n,freed=a in f.freed) for a,n in f.allocations.items()])
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def call(label,entry,this=0,args=()):
        report['pending']=dict(label=label,entry=f'{entry:08X}');save();value=f.call(entry,this,args)
        report['stages'].append(dict(label=label,entry=f'{entry:08X}',eax=value,instructions=sum(p.visits.values())));report.pop('pending');save();return value
    def snapshot(a):
        n=f.allocations[a];v=p.uint(a)
        return dict(address=a,allocationBytes=n,vtable=f'{v:08X}',baseSlots=[f'{p.uint(v+4*i):08X}' for i in range(7)],bytes=bytes(p.mu.mem_read(a,n)).hex())
    try:
        call('matrix-initializer',0x6d38e0)
        engine=p.allocate(0x158);scene=p.allocate(0x54);p.mu.mem_write(engine,bytes(0x158));p.mu.mem_write(scene,bytes(0x54));p.put_uint(0x755274,engine)
        root=call('root-factory',0x421e20);p.put_uint(engine+0x18,scene);p.put_uint(scene+0x14,root)
        obj=call('factory',0x4014c0);report['initial']=snapshot(obj);assert f.allocations[obj]==0x28 and p.uint(obj)==0x6f6300;save()
        assert call('rtti',p.uint(p.uint(obj)+0x10),obj)==0x74e548
        clone=call('clone',p.uint(p.uint(obj)+8),obj);assert clone and clone!=obj;report['clone']=snapshot(clone);save()
        for label,a in [('delete-clone',clone),('delete-original',obj),('delete-root',root)]:
            call(label,p.uint(p.uint(a)),a,(1,));assert a in f.freed
        report['remainingAllocations']=[a for a in f.allocations if a not in f.freed];report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error));save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',classBytes=report['initial']['allocationBytes'],stages=len(report['stages']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(args[1]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
