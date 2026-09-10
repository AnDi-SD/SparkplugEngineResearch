#!/usr/bin/env python3
"""Original completion marks, state/audio tag routing and finite audio gate.

PC full controller routes use original base/Bird state callbacks. PS2 stops at
SQ child entries or executes actual scalar leaves; no replacement game returns.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime


def execute(c):
    ispc=c['platform']=='pc';kind=c['kind'];code=c.get('eventCode',11);shift=0 if ispc else 12
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;area=p.allocate(0x4000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(0x2a6da0,0x1f4),(0x2c8e10,8),(0x100390,0x30),(0x100610,0xd0),(0x100810,8),(0x48c5e0,0x28),(0x49a9e0,0x4c),(0x49a350,0x4c),(0x495530,0x44),(0x3c7de0,0x220)])
        area=0x21000000;p.map(area,0x4000);p.map(0x22000000,0x1000);p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        read,write,put=p.read,p.write,p.put_uint
    write(area,b'\xa5'*0x4000)
    parent,char,machine,state,audio,move,event,tag,name,game,container=tuple(area+i*0x400 for i in range(11))
    put(parent,0x6f670c if ispc else 0x49b450);put(parent+4,0)
    put(parent+0x124+shift,char if c.get('character',True) else 0)
    put(parent+0x178+shift,c.get('old',7));put(parent+0x17c+shift,c.get('recent',9));put(parent+0x180+shift,c.get('counter',2))
    put(event,code);put(event+0x1c,c.get('handle',tag))
    put(tag+0x10,name);write(name,c.get('name','event_takeoff').encode('ascii')+b'\0')
    put(char+0x124+shift,machine);put(char+0x144+shift,audio if c.get('audio',False) else 0);put(char+0x12c+shift,move if c.get('move',True) else 0)
    write(move+0x20d+shift,bytes([c.get('variant',255)]))
    index=c.get('stateIndex',0);assert index in (0,41)
    put(machine+0x130+shift,index);put(machine+0x148+shift+index*4,state)
    bird=c.get('state','base')=='bird'
    put(state,(0x6f88f0 if ispc else 0x49a350) if bird else (0x6f81b0 if ispc else 0x49a9e0));put(state+0x14,char)
    put(audio,0x701b94 if ispc else 0x495530)
    put(game+0x1b0,c.get('gameState',38))
    if ispc:put(0x755294,game)
    else:p.map(0x49fd4c,4);put(0x49fd4c,game)
    forwards=[];callbacks=[];wanted=[];entry=0x4fb780;this=parent;args=(event,);stop=None;psentry=0x2a6db4;psstops=[0x2a6f84];psregs=dict(A0=parent,A1=event,V1=11)
    # Original intrusive forward list fields; callbacks are real spBaseObject
    # v1 methods and both platforms have their actual immutable vtables.
    if kind=='forward':
        count=c['count'];assert 0<=count<=3
        head=container+0x40 if ispc else container+4
        links=[container+0x100+i*0x20 for i in range(count)]
        subscribers=[area+0x3000+i*0x100 for i in c.get('order',list(range(count)))];assert len(subscribers)==count
        put(parent+4,container if c.get('container',True) else 0)
        if ispc:put(container+4,head)
        put(head+(0 if ispc else 4),links[0] if links else head)
        for i,(link,obj) in enumerate(zip(links,subscribers)):
            put(link+(0 if ispc else 4),links[i+1] if i+1<count else head);put(link+8,obj);put(obj,0x6daeb8 if ispc else 0x48c5e0)
        wanted=[dict(kind='forward',this=obj,event=event) for obj in subscribers] if c.get('container',True) else []
        entry=0x40f960;psentry=0x100624;psstops=[0x1006c8];psregs=dict(A0=parent,A1=event)
    before=read(area,0x4000);expected=bytearray(before)
    def expect(a,v):struct.pack_into('<I',expected,a-area,v&0xffffffff)
    if kind=='marks':
        assert code in (3,9);h=c['handle'];old=c.get('old',7);recent=c.get('recent',9)
        if old==h:expect(parent+0x178+shift,0);expect(parent+0x180+shift,c.get('counter',2)-1)
        elif recent==h:expect(parent+0x17c+shift,0);expect(parent+0x180+shift,c.get('counter',2)-1)
        expect(parent+(0x174 if code==3 else 0x170)+shift,h)
        if code==3:
            wanted=[dict(kind='forward-entry',this=parent,event=event)]
            if not ispc:psstops=[0x100610]
    elif kind=='tag':
        assert code==11
        if c.get('character',True):
            wanted.append(dict(kind='state',this=state,event=event))
            if bird:
                if ispc:
                    if c.get('name','event_takeoff')=='event_takeoff':expected[char-area+0x26c]=0
                else:psstops=[0x2e5480]
            if c.get('audio',False) and (ispc or not bird):
                wanted.append(dict(kind='audio',this=audio,event=event,variant=c.get('variant',255) if c.get('move',True) else 0))
                stop=0x586c60;psstops=[0x3c7de0]
    elif kind=='audio-gate':
        raw=c['gameState'];signed=raw-(1<<32) if raw&0x80000000 else raw;eligible=signed<=37 or 41<=signed<=49 or signed==70
        entry=0x586c60;this=audio;args=(event,c['variant']);stop=0x582690 if eligible else None
        psentry=0x3c7df8;psstops=[0x3cb8e0 if eligible else 0x3c7fec];psregs=dict(S2=audio,A1=event,A2=c['variant'])
    elif kind=='ignored':assert code not in (3,9,11,28)
    elif kind!='forward':raise ValueError(kind)
    def observe(u,a,n,user):
        if ispc:
            receiver=p.reg('ECX');argument=p.uint(p.reg('ESP')+4)
            if kind=='marks' and a==0x40f960:callbacks.append(dict(kind='forward-entry',this=receiver,event=argument))
            elif kind=='forward' and a==0x5b7a00:callbacks.append(dict(kind='forward',this=receiver,event=argument))
            elif kind=='tag' and a==(0x517c40 if bird else 0x5b7a00):callbacks.append(dict(kind='state',this=receiver,event=argument))
            elif kind=='tag' and a==0x586c60:callbacks.append(dict(kind='audio',this=receiver,event=argument,variant=p.uint(p.reg('ESP')+8)))
        else:
            if kind=='marks' and a==0x100610:callbacks.append(dict(kind='forward-entry',this=p.reg('A0'),event=p.reg('A1')))
            elif kind=='forward' and a==0x100810:callbacks.append(dict(kind='forward',this=p.reg('A0'),event=p.reg('A1')))
            elif kind=='tag' and a==(0x2e5480 if bird else 0x2c8e10):callbacks.append(dict(kind='state',this=p.reg('A0'),event=p.reg('A1')))
            elif kind=='tag' and a==0x3c7de0:callbacks.append(dict(kind='audio',this=p.reg('A0'),event=p.reg('A1'),variant=p.reg('A2')))
    (p.mu if ispc else p.u).hook_add(p.uc.UC_HOOK_CODE if ispc else __import__('unicorn').UC_HOOK_CODE,observe)
    if ispc:
        p.run(entry,this,args,stop_at=stop)
        result=dict(entry=f'{entry:08X}',stop=f'{p.reg("EIP"):08X}',completion='original return' if stop is None else 'real callee entry;guest discarded',blocks=sum(p.visits.values()))
        if kind=='tag' and stop==0x586c60:
            # PcBlocks can stop at block entry before UC_HOOK_CODE observers.
            # Read the original boundary registers/stack after the stop instead.
            boundary=dict(kind='audio',this=p.reg('ECX'),event=p.uint(p.reg('ESP')+4),variant=p.uint(p.reg('ESP')+8))
            if not callbacks or callbacks[-1].get('kind')!='audio':callbacks.append(boundary)
            else:assert callbacks[-1]==boundary
    else:
        for k,v in psregs.items():p.reg(k,v)
        result=p.run(psentry,psstops,timeout_us=500000);result['initialRegisters']=psregs
    assert callbacks==wanted,(callbacks,wanted);result['callbacks']=callbacks
    if kind=='audio-gate':
        result['eligible']=eligible
        if eligible:
            keyptr=p.uint(p.reg('ESP')+4) if ispc else p.reg('A2')
            values=[p.uint(keyptr+i) for i in (0,4,8)]
            assert values==[name,c['variant'],1000000],values
            assert (p.reg('ECX') if ispc else p.reg('A1'))==audio+0x128+shift
            result['lookupKey']=values
    after=read(area,0x4000)
    assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:16]
    result.update(borrowedStorageBytes=0x4000,beforeSha256=hashlib.sha256(before).hexdigest().upper(),afterSha256=hashlib.sha256(after).hexdigest().upper(),changes=[dict(offset=i,before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b])
    return result


def guest(output,selection):
    output=Path(output).resolve();selection=Path(selection).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    cases=json.loads(selection.read_text())['cases'];assert 0<len(cases)<=100
    started=time.perf_counter();report=dict(kind='original-character-animation-event-routing',status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selectionSha256=hashlib.sha256(selection.read_bytes()).hexdigest().upper(),cases=[])
    output.parent.mkdir(parents=True,exist_ok=True)
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in cases:
        report['pending']=c;save()
        try:result=dict(input=c,status='passed',**execute(c))
        except Exception as error:result=dict(input=c,status='blocked',error=str(error),traceback=traceback.format_exc())
        report['cases'].append(result);report.pop('pending');save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='passed' if not failed else 'partial';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
