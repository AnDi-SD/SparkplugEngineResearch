#!/usr/bin/env python3
"""Original wxEntity cached flag calculation and own copy payload.

PC executes the complete methods. PS2 integer/predicate paths and own copy
prefix execute;distance accumulation is excluded and its comparison tail is
tested separately with explicit finite sums. These are not full PS2 frames.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix,unicorn
from capture_native_ranges import EXPECTED
import probe_pc_animation_lifecycle as lifetime


def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    report=dict(kind='original-wx-entity-common-methods',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Literal wxEntity/entity-record/cached list interfaces. Predicates use original constant leaves;timer-byte getter is original. No cache construction,full scene,normal entity-record binding or R5900 accumulator implementation. Copy PS2 starts after inherited named copy success and stops at real clone-manager consumer.',
        limits='Fresh guests.PC block100k/2s;PS2 scalar2000/100ms;30s outer/default64KiB PC arena.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    patterns=([],[True],[False],[True,True],[True,False,True],[False,True],[True,False],[False,False])
    cases=[dict(kind='flags',enabled=True,children=x,pauseEnabled=False,pause=0) for x in patterns]
    cases += [dict(kind='flags',enabled=False,children=x,pauseEnabled=False,pause=0) for x in ([],[False,True])]
    cases += [dict(kind='flags',enabled=True,children=[],pauseEnabled=True,pause=x) for x in (0,1,2,255)]
    cases += [dict(kind='flags',enabled=True,children=[],pauseEnabled=False,pause=255)]
    cases += [dict(kind='distance',point=[3,4,0],limit=bits(x)) for x in (24,25,26)]
    cases += [dict(kind='distance',point=[0,0,0],limit=bits(x)) for x in (-1,0)]
    cases += [dict(kind='distance',point=[3,4,0],limit=0x7fc00000)]
    cases += [dict(kind='copy',flags=f,word=w) for f,w in (([0,0,0],0),([1,1,0],0x7f7fffff),([255,128,3],0x7fc12345))]
    def pc(case):
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
        p=f.p;obj=p.allocate(0x124);p.mu.mem_write(obj,b'\xa5'*0x124);p.put_uint(obj,0x6f4eb8);p.put_uint(obj+0x10,0)
        if case['kind']=='copy':
            target=p.allocate(0x124);p.mu.mem_write(target,b'\x5a'*0x124);p.put_uint(target,0x6f4eb8);p.put_uint(target+0x10,0)
            for off,value in zip((0x28,0x38,0x39),case['flags']):p.mu.mem_write(obj+off,bytes([value]))
            p.put_uint(obj+0x3c,case['word']);source_before=bytes(p.mu.mem_read(obj,0x124));before=bytes(p.mu.mem_read(target,0x124))
            value=f.call(0x4d9700,obj,(target,));expected=bytearray(before)
            for off in (0x28,0x38,0x39):expected[off]=source_before[off]
            expected[0x3c:0x40]=source_before[0x3c:0x40]
            assert value&255==1 and bytes(p.mu.mem_read(target,0x124))==expected and bytes(p.mu.mem_read(obj,0x124))==source_before
            return dict(result=True,changedOffsets=[i for i,(a,b) in enumerate(zip(before,expected)) if a!=b],payload=dict(flags=case['flags'],word=p.uint(target+0x3c)),blocks=sum(p.visits.values()))
        record=p.allocate(0x28);p.mu.mem_write(record,b'\xa5'*0x28);p.put_uint(obj+0x24,record)
        point=p.allocate(12);p.mu.mem_write(point,bytes(12));p.put_uint(obj+0x3c,case.get('limit',0x7f7fffff));p.put_uint(record+0x24,0)
        p.mu.mem_write(obj+0x28,b'\1');p.mu.mem_write(obj+0x38,bytes([case.get('enabled',False)]));p.mu.mem_write(obj+0x39,bytes([case.get('pauseEnabled',False)]))
        timer=p.allocate(0x44);p.mu.mem_write(timer,bytes(0x44));p.mu.mem_write(timer+0x40,bytes([case.get('pause',0)]));p.put_uint(0x755298,timer)
        head=p.allocate(12);nodes=[];receivers=[];events=[]
        for accept in case.get('children',[]):
            link=p.allocate(12);child=p.allocate(0xc0);vt=p.allocate(0x14)
            p.put_uint(child+0xb4,vt);p.put_uint(vt+0x10,0x4f3df0 if accept else 0x4a1bf0);p.put_uint(link+8,child);nodes.append(link);receivers.append(child+0xb4)
        chain=[head,*nodes]
        for i,link in enumerate(chain):p.put_uint(link,chain[(i+1)%len(chain)]);p.put_uint(link+4,chain[(i-1)%len(chain)])
        p.put_uint(obj+0x30,head)
        if case['kind']=='distance':
            transform=p.allocate(0x80);p.put_uint(record+0x24,transform);p.mu.mem_write(transform+0x74,struct.pack('<3f',*case['point']))
        def observe(u,a,n,user):
            if p.reg('ECX') in receivers:events.append(receivers.index(p.reg('ECX')))
        for address in (0x4f3df0,0x4a1bf0):p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=address,end=address)
        before=bytes(p.mu.mem_read(obj,0x124));record_before=bytes(p.mu.mem_read(record,0x28));f.call(0x4da930,obj,(point,))
        flags=p.uint(record+12);expected=0xffffff00
        if case.get('enabled') and all(case['children']):expected|=2
        if case.get('pauseEnabled') and case['pause']:expected|=8
        if case['kind']=='distance' and case['limit']!=0x7fc00000:
            if sum(x*x for x in case['point'])>struct.unpack('<f',struct.pack('<I',case['limit']))[0]:expected|=16
        assert flags==expected and bytes(p.mu.mem_read(obj,0x124))==before
        desired=bytearray(record_before);desired[12:16]=struct.pack('<I',expected);assert bytes(p.mu.mem_read(record,0x28))==desired
        visited=[]
        if case.get('enabled'):
            for i,value in enumerate(case['children']):
                visited.append(i)
                if not value:break
        assert events==visited
        return dict(flags=flags,predicateOrder=events,blocks=sum(p.visits.values()),entityUnchanged=True,onlyRecordWordCChanged=True)
    def ps2(case):
        if case['kind']=='copy':
            p=Ps2ScalarPrefix([(0x286ad4,0x54)]);p.map(0x21000000,0x3000);p.map(0x49f000,4096)
            source,target,manager=0x21000000,0x21001000,0x21002000
            p.write(source,b'\xa5'*0x130);p.write(target,b'\x5a'*0x130)
            for off,value in zip((0x28,0x38,0x39),case['flags']):p.write(source+off,bytes([value]))
            p.put_uint(source+0x3c,case['word']);p.put_uint(0x49f810,manager)
            for n,v in (('S1',source),('S0',target),('GP',0x4a4170)):p.reg(n,v)
            before=p.read(target,0x130);r=p.run(0x286ad4,[0x104f00]);expected=bytearray(before)
            for off in (0x28,0x38,0x39):expected[off]=p.read(source+off,1)[0]
            expected[0x3c:0x40]=p.read(source+0x3c,4)
            assert p.read(target,0x130)==expected and p.reg('A0')==manager and p.reg('A1')==0x796a1869
            r.update(payload=dict(flags=case['flags'],word=p.uint(target+0x3c)),cloneManagerClassHash=p.reg('A1'));return r
        distance=case['kind']=='distance'
        ranges=[(0x286488,0x208),(0x14e660,8),(0x2c8df0,0x20),(0x28f020,8)]
        p=Ps2ScalarPrefix(ranges);p.map(0x21000000,0x5000);p.map(0x22000000,4096);p.map(0x49f000,4096)
        obj,record,point,timer,head=0x21000000,0x21001000,0x21002000,0x21003000,0x21000030
        p.write(obj,b'\xa5'*0x130);p.write(record,b'\xa5'*0x28);p.put_uint(obj+0x24,record);p.put_uint(record+0x24,0)
        p.put_uint(obj+0x3c,case.get('limit',0x7f7fffff));p.write(obj+0x28,b'\1');p.write(obj+0x38,bytes([case.get('enabled',False)]));p.write(obj+0x39,bytes([case.get('pauseEnabled',False)]))
        p.write(timer+0x40,bytes([case.get('pause',0)]));p.put_uint(0x49fc80,timer)
        nodes=[];receivers=[];events=[]
        for i,accept in enumerate(case.get('children',[])):
            link=0x21000400+i*0x100;child=link+0x10;vt=link+0x20
            p.put_uint(child,vt);p.put_uint(vt+0x6c,0x2c8e00 if accept else 0x2c8df0);p.put_uint(link+8,child);nodes.append(link);receivers.append(child)
        chain=[head,*nodes]
        for i,link in enumerate(chain):p.put_uint(link+4,chain[(i+1)%len(chain)]);p.put_uint(link,chain[(i-1)%len(chain)])
        for n,v in (('A0',obj),('S4',obj),('S5',point),('GP',0x4a4170),('SP',0x22000800)):p.reg(n,v)
        before=p.read(obj,0x130);record_before=p.read(record,0x28)
        if distance:
            p.reg('S3',0xffffff00);p.reg('F0',bits(sum(x*x for x in case['point'])));p.reg('F6',case['limit'])
            entry=0x286650
        else:entry=0x286488
        def observe(u,a,n,user):
            if a in (0x2c8e00,0x2c8df0):events.append(receivers.index(p.reg('A0')))
        p.u.hook_add(unicorn.UC_HOOK_CODE,observe)
        r=p.run(entry,[0x286690]);flags=p.uint(record+12)
        assert p.read(obj,0x130)==before
        expected=bytearray(record_before);expected[12:16]=struct.pack('<I',flags);assert p.read(record,0x28)==expected
        r.update(flags=flags,predicateOrder=events,scope='comparison tail only;finite squared distance supplied explicitly,no PS2 accumulation execution' if distance else 'cached list and timer predicates plus sentinel path before excluded restore frame')
        return r
    try:
        for case in cases:
            report['pending']=case;save();a=pc(case);report['pendingPc']=a;save()
            b=None if case['kind']=='distance' and case['limit']==0x7fc00000 else ps2(case)
            if b is not None:
                keys=('payload',) if case['kind']=='copy' else ('flags','predicateOrder')
                assert all(a[k]==b[k] for k in keys),(case,a,b)
            report['cases'].append(dict(input=case,pc=a,ps2=b));report.pop('pending');report.pop('pendingPc');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pcCases=len(cases),pairedCachedOrCopy=18,ps2ComparisonTails=5,seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
