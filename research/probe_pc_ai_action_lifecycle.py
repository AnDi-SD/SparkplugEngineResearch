#!/usr/bin/env python3
"""Bounded original AIAction lifecycle, retaining each completed stage."""
from pathlib import Path
import hashlib,json,sys,time
from pc_instruction_emulator import ROOT,run_bounded,execution_limits
from probe_pc_task_timer import TimerFixture


def guest(class_name,output,factory_profile='micro',context='none'):
    execution_limits(factory_profile)
    if context not in ('none','empty-scene'):raise ValueError('Explicit none or empty-scene context required')
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    source=ROOT/'local-data/results/native-cycle-20260910-1900/ai-action/catalog-family.json'
    record=next(x for x in json.loads(source.read_text()) if x['className']==class_name)
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter();f=TimerFixture();p=f.p;f.time(1200)
    report=dict(kind='original-pc-ai-action-lifecycle',className=class_name,status='running',record=record,stages=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),catalogSha256=hashlib.sha256(source.read_bytes()).hexdigest().upper(),
        executableSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        limits='micro100000 instructions/2s per original operation;30s child;64KiB heap/32KiB allocation;fresh guest per class',
        boundaries='Existing TimerFixture allocator/SEH,clock1200/divisor1/refreshfalse and clone-map observations. Optional borrowed context is declared separately; no OS or substituted game constructor/search.',factoryReached=False,wholeClassClosed=False)
    report['factoryProfile']=dict(name=factory_profile,limits=execution_limits(factory_profile),reason='Explicit fresh-guest escalation after recorded micro cap' if factory_profile!='micro' else 'Default bounded operation')
    def save():
        report.update(seconds=time.perf_counter()-started,arenaBytes=p.allocated,lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{a:08X}' for a in p.tail],
            allocations=[dict(address=a,bytes=n,freed=a in f.freed) for a,n in f.allocations.items()],
            lastInstructionCount=sum(p.visits.values()),lastHotAddresses=[dict(address=f'{a:08X}',visits=n) for a,n in p.visits.most_common(16)])
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def call(label,address,this=0,args=()):
        p.execution_profile=factory_profile if label=='factory' else 'micro'
        report['pending']=dict(label=label,address=f'{address:08X}',this=this,args=args);save()
        value=f.call(address,this,args);report['stages'].append(dict(label=label,address=f'{address:08X}',eax=value,instructions=sum(p.visits.values())))
        report.pop('pending');save();return value
    def snapshot(a):
        n=f.allocations[a];assert n>=0x10;vt=p.uint(a)
        return dict(address=a,allocationBytes=n,bytes=bytes(p.mu.mem_read(a,n)).hex(),vtable=f'{vt:08X}',slots=[f'{p.uint(vt+4*i):08X}' for i in range(17)])
    try:
        if context=='empty-scene':
            # Actual Node factory/search, with literal external core/scene
            # pointers. No scene initialize, game startup or fake Find result.
            call('matrix-static-initializer',0x6d38e0)
            engine=p.allocate(0x158);scene=p.allocate(0x54)
            p.mu.mem_write(engine,bytes(0x158));p.mu.mem_write(scene,bytes(0x54));p.put_uint(0x755274,engine)
            root=call('borrowed-root-factory',0x421e20)
            p.put_uint(engine+0x18,scene);p.put_uint(scene+0x14,root)
            report['declaredContext']=dict(kind=context,engineStorage=engine,sceneStorage=scene,originalRoot=root,
                scope='Opaque borrowed engine+18->scene+14->actual unnamed empty Node; no original engine/scene factory or initialize claim')
            save()
        obj=call('factory',record['pcFactory']);report['factoryReached']=True;report['initial']=snapshot(obj)
        report['factoryVisits']={f'{a:08X}':n for a,n in sorted(p.visits.items())};save()
        assert call('rtti',p.uint(p.uint(obj)+0x10),obj)==record['pcRecord']
        clone=call('clone',p.uint(p.uint(obj)+8),obj);assert clone and clone!=obj;report['clone']=snapshot(clone);report['clonePairs']=f.clone_pairs;save()
        for label,a in [('delete-clone',clone),('delete-original',obj)]:
            call(label,p.uint(p.uint(a)),a,(1,));assert a in f.freed
        if context=='empty-scene':
            call('delete-borrowed-root',p.uint(p.uint(root)),root,(1,));assert root in f.freed
        report['remainingAllocations']=[a for a in f.allocations if a not in f.freed];report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),errorType=type(error).__name__);save()
        print(json.dumps(dict(status='blocked',className=class_name,factoryReached=report['factoryReached'],pending=report.get('pending'),error=str(error))));return 1
    print(json.dumps(dict(status='passed',className=class_name,bytes=report['initial']['allocationBytes'],stages=len(report['stages']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
