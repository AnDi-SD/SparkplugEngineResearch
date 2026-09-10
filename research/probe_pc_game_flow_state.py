#!/usr/bin/env python3
"""Bounded original wxGameFlowState lifecycle and narrow member/global operations."""
from pathlib import Path
import hashlib,json,struct,sys,time,traceback
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_task_timer import TimerFixture

def main(mode,output):
    if mode not in ('image-prerequisite','borrowed-input'):raise ValueError('Explicit prerequisite mode required')
    target=Path(output).resolve()
    if not target.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local research output required')
    target.parent.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();f=TimerFixture();p=f.p;f.time(1200)
    report=dict(kind='original-pc-game-flow-state',status='running',profile='micro',prerequisiteMode=mode,
                scope='Original factory/destructor and explicit narrow operations, not game startup/UI',
                clockInput=dict(rawTicks=1200,divisor=1,refresh=False),
                sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
                inputs=dict(executableSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper()),
                stages=[],wholeGameStartup=False)
    if mode=='borrowed-input':
        # Literal external storage, not a fabricated runtime class or successful
        # replacement of its constructor. This class reads only member18 here.
        owner=p.allocate(0x24);borrowed=p.allocate(16)
        p.mu.mem_write(owner,b'\xcc'*0x24);p.mu.mem_write(borrowed,b'\x7d'*16)
        p.put_uint(owner+0x18,borrowed);p.put_uint(0x7552a0,owner)
        report['declaredPrerequisite']=dict(globalAddress='007552A0',opaqueOwner=owner,
            bytes=0x24,borrowedMember18=borrowed,borrowedBytes=16,
            scope='Explicit external storage; no original class/default/startup claim')
    def save():
        report.update(seconds=time.perf_counter()-started,arenaBytes=p.allocated,
                      lastInstructionCount=sum(p.visits.values()),
                      allocations=[dict(address=a,bytes=n,freed=a in f.freed) for a,n in f.allocations.items()],
                      lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{a:08X}' for a in p.tail])
        target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def call(label,address,this=0,args=()):
        value=f.call(address,this=this,args=args)
        report['stages'].append(dict(label=label,address=f'{address:08X}',eax=value,
            instructions=sum(p.visits.values()),this=this,args=list(args)))
        save();return value
    def snapshot(obj):
        timer=p.uint(obj+0x2c);borrowed=p.uint(obj+0x30)
        return dict(address=obj,bytes=bytes(p.mu.mem_read(obj,0x3c)).hex(),vtable=f'{p.uint(obj):08X}',
                    timer=timer,timerBytes=bytes(p.mu.mem_read(timer,0x3c)).hex() if timer else None,
                    borrowed30=borrowed,fields18_20=[p.uint(obj+i) for i in (0x18,0x1c,0x20)])
    try:
        report['imageTableBefore']=[p.uint(0x742de4+4*i) for i in range(56)]
        obj=call('factory',0x5d6dc0)
        assert f.allocations.get(obj)==0x3c and p.uint(obj)==0x70f188
        report['initial']=snapshot(obj);save()
        rows=[]
        for args in [(1,2,3),(0xffffffff,0,0x80000000),(0,0xffffffff,0xffffffff),(0xffffffff,0xffffffff,0xffffffff)]:
            before=bytes(p.mu.mem_read(obj,0x3c))
            call('set-three-non-sentinel-fields',0x5d6850,obj,args)
            after=bytes(p.mu.mem_read(obj,0x3c))
            rows.append(dict(args=list(args),before=before.hex(),after=after.hex()))
            expected=bytearray(before)
            for offset,value in zip((0x18,0x1c,0x20),args):
                if value!=0xffffffff:struct.pack_into('<I',expected,offset,value)
            assert bytes(expected)==after
        report['memberCases']=rows
        # Explicit synthetic source IDs establish permutation, not game startup values.
        ids=[0x60000000+i for i in range(56)]
        for i,value in enumerate(ids):p.put_uint(0x742de4+4*i,value)
        p.mu.mem_write(0x768250,b'\xa5'*(56*4))
        call('reset-global-mapping-directed',0x5d6880)
        actual=[p.uint(0x768110+4*i) for i in range(56)]
        report['mapping']=dict(sourceAddress='00742DE4',outputAddress='00768110',source=ids,output=actual,
                               sourceIndexes=[x-0x60000000 for x in actual],
                               clearedAddress='00768250',clearedBytes=bytes(p.mu.mem_read(0x768250,56*4)).hex())
        assert sorted(report['mapping']['sourceIndexes'])==list(range(56))
        assert bytes(p.mu.mem_read(0x768250,56*4))==bytes(56*4)
        # Clone constructor defaults its own state. Existing fixture records clone-map boundary.
        clone=call('clone',0x5d6e20,obj)
        assert clone!=obj and f.allocations.get(clone)==0x3c
        report['clone']=snapshot(clone);report['clonePairs']=f.clone_pairs
        assert report['clone']['fields18_20']==[0xffffffff]*3
        assert p.uint(clone+0x2c)!=p.uint(obj+0x2c)
        save()
        for label,value in [('delete-clone',clone),('delete-original',obj)]:
            timer=p.uint(value+0x2c)
            call(label,0x5d6bc0,value,(1,))
            assert value in f.freed and timer in f.freed
        report['status']='passed'
        report['remainingOwnershipScope']='Any retained lazy singleton belongs to the original global prerequisite; not attributed to GameFlowState teardown.'
        save()
    except Exception as error:
        report['status']='blocked';report['error']=str(error);report['errorType']=type(error).__name__
        save();print(json.dumps(dict(status=report['status'],error=report['error'],output=str(target))),flush=True)
        return 1
    print(json.dumps(dict(status=report['status'],stages=len(report['stages']),seconds=report['seconds'],
                         allocations=len(f.allocations),freed=len(f.freed),output=str(target))),flush=True)
    return 0

if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(main(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
