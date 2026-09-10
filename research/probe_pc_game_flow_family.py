#!/usr/bin/env python3
"""One original GameFlowState descendant lifecycle per bounded fresh process.

Reuses the proved base prerequisite and timer fixture. Captures construction
before clone/teardown, retaining useful evidence if a later stage is blocked.
No game/HUD/API callee is invented to make an unavailable constructor succeed.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_task_timer import TimerFixture


def guest(class_name,output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    catalog_path=ROOT/'local-data/results/native-cycle-20260910-1900/catalog/pc-architecture.json'
    catalog=json.loads(catalog_path.read_text(encoding='utf-8'))
    index={x['class_hash']:x for x in catalog['registered_types']}
    record=next(x for x in index.values() if x['class_name']==class_name)
    current=record;seen=set()
    while current['class_hash']!=0x11521afa:
        assert current['class_hash'] not in seen,'Inheritance cycle';seen.add(current['class_hash'])
        current=index[current['base_class_hash']]
    factory=record['constructor_arg5_va'];assert factory,'Abstract/no registered factory'
    f=TimerFixture();p=f.p;f.time(1200);started=time.perf_counter()
    owner=p.allocate(0x24);borrowed=p.allocate(16)
    p.mu.mem_write(owner,b'\xcc'*0x24);p.mu.mem_write(borrowed,bytes(16));p.put_uint(owner+0x18,borrowed);p.put_uint(0x7552a0,owner)
    report=dict(kind='original-pc-game-flow-descendant-lifecycle',status='running',className=class_name,record=record,stages=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        catalogSha256=hashlib.sha256(catalog_path.read_bytes()).hexdigest().upper(),
        executableSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        profile='micro100000 instructions/2s per operation;30s child;fresh guest per class',
        declaredInputs=dict(userInputGlobal='007552A0',opaqueOwner=owner,ownerBytes=0x24,borrowedMember18=borrowed,
            borrowedBytes=16,clockTicks=1200,clockDivisor=1,clockRefresh=False),
        boundaries='Existing TimerFixture allocator/SEH/clock/clone-map recording only; explicit borrowed UserInput storage. No full game/HUD startup or substitute class constructor.',
        factoryReached=False,wholeClassClosed=False)
    output.parent.mkdir(parents=True,exist_ok=True)
    def save():
        report.update(seconds=time.perf_counter()-started,arenaBytes=p.allocated,
            allocations=[dict(address=a,bytes=n,freed=a in f.freed) for a,n in f.allocations.items()],
            lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{a:08X}' for a in p.tail],lastReason=getattr(p,'reason',None))
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def call(label,address,this=0,args=()):
        report['pending']=dict(label=label,address=f'{address:08X}',this=this,args=args);save()
        value=f.call(address,this,args)
        report['stages'].append(dict(label=label,address=f'{address:08X}',this=this,args=args,eax=value,instructions=sum(p.visits.values())))
        report.pop('pending',None);save();return value
    def snapshot(a):
        size=f.allocations.get(a);assert size and 0x3c<=size<=0x8000
        vt=p.uint(a);table=[p.uint(vt+4*i) for i in range(13)]
        return dict(address=a,allocationBytes=size,bytes=bytes(p.mu.mem_read(a,size)).hex(),vtable=f'{vt:08X}',
                    baseSlots=[f'{x:08X}' for x in table],timer=p.uint(a+0x2c),borrowed30=p.uint(a+0x30))
    try:
        obj=call('factory',factory);report['factoryReached']=True;report['initial']=snapshot(obj)
        report['factoryVisits']={f'{a:08X}':n for a,n in sorted(p.visits.items())};save()
        vt=p.uint(obj);rtti=call('rtti-getter',p.uint(vt+0x10),obj)
        assert rtti==record['registration_object_va'],(hex(rtti),hex(record['registration_object_va']))
        clone=call('clone',p.uint(vt+8),obj);assert clone and clone!=obj
        report['clone']=snapshot(clone);report['clonePairs']=f.clone_pairs;save()
        for label,a in [('delete-clone',clone),('delete-original',obj)]:
            timer=p.uint(a+0x2c);call(label,p.uint(p.uint(a)),a,(1,));assert a in f.freed and timer in f.freed
        report['remainingAllocations']=[a for a in f.allocations if a not in f.freed]
        subscription=p.uint(0x75537c)
        if subscription:
            report['remainingSubscriptionManager']=dict(address=subscription,
                bytes=bytes(p.mu.mem_read(subscription,0x20)).hex(),
                treeHead=p.uint(subscription+0x18),groupCount=p.uint(subscription+0x1c))
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),errorType=type(error).__name__);save()
        print(json.dumps(dict(className=class_name,status='blocked',factoryReached=report['factoryReached'],pending=report.get('pending'),error=str(error))));return 1
    print(json.dumps(dict(className=class_name,status='passed',bytes=report['initial']['allocationBytes'],stages=len(report['stages']),
                         allocations=len(f.allocations),freed=len(f.freed),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
