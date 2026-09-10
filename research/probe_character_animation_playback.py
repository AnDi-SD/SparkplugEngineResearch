#!/usr/bin/env python3
"""Original wxAnimationController -> spActor controls and PS2 start preparation.

Borrowed object fields and slots only. Game events/binder are real stop entries,
never success seams. PC original Start rejection can return through the wrapper.
PS2 SQ/LQ transactions remain explicit post-prologue components.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime


def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]


def execute(c):
    ispc=c['platform']=='pc';kind=c['kind'];slots=c.get('slots',[])
    assert len(slots)<=4
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;area=p.allocate(0x3000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(0x2a6cd0,0xc8),(0x115fa0,0x1b4),(0x1164a0,0x538)])
        area=0x21000000;p.map(area,0x3000);p.map(0x22000000,0x1000);p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        read,write,put=p.read,p.write,p.put_uint
    write(area,b'\xa5'*0x3000)
    parent,actor,rows,request,animation,manager,engine=area,area+0x400,area+0x800,area+0x1000,area+0x1400,area+0x1800,area+0x1c00
    shift=0 if ispc else 12
    put(parent,0x6f670c if ispc else 0x49b450);put(parent+0x128+shift,actor)
    put(parent+0x178+shift,c.get('old',0));put(parent+0x17c+shift,c.get('recent',0x12345678));put(parent+0x180+shift,c.get('counter',0xffffffff))
    put(actor+0x28,rows);put(actor+0x2c,len(slots))
    def key(v):return animation if v=='animation' else v
    for i,s in enumerate(slots):
        a=rows+i*0x60;put(a,key(s['key']));put(a+0x48,s.get('uses',0));write(a+0x4c,bytes([s.get('running',0)]));put(a+0xc,bits(.625))
    handle=key(c.get('handle',0x13572468))
    if kind=='actor-start':
        assert handle==animation
        for off,v in ((0,animation),(4,c.get('mode',3)),(0x10,c['fade']),(0x28,0),(0x2c,0xfedcba98)):
            put(request+off,v)
        write(request+8,b'\xff')
        for off,v in ((0xc,.25),(0x14,c['fadeIn']),(0x18,7.),(0x1c,c['fadeOut']),(0x20,-.375),(0x24,.75),(0x30,2.),(0x34,c.get('initial',.5))):put(request+off,bits(v))
        put(animation+0x14,bits(c.get('duration',1.)));put(animation+0x18,c.get('priority',0x123))
        put(manager+0x10,c.get('frame',0xfedcba98))
        if ispc:put(0x75f880,manager);put(0x755274,engine)
        else:
            for g,v in ((0x49f858,manager),(0x49f850,engine)):p.map(g,4);put(g,v)
    before=read(area,0x3000);expected=bytearray(before)
    def expect(a,v,width=4):expected[a-area:a-area+width]=(v&((1<<(width*8))-1)).to_bytes(width,'little')
    result={};entry=stop=None;args=();this=actor;psregs={};psentry=None;psstops=None
    first=next((i for i,s in enumerate(slots) if key(s['key'])==handle),None)
    if kind in ('control-start','control-rejected-start'):
        args=(handle,c['mode'],c['fade'],c['interrupt']);this=parent;entry=0x4fb620
        interrupt=c['interrupt']!=0 and c.get('old',0)!=0
        if kind=='control-rejected-start':
            assert ispc and all(s.get('uses',0)!=0 and key(s['key'])!=handle and key(s['key'])!=c.get('old',0) for s in slots)
            stop=None;expect(parent+0x180,(c.get('counter',0xffffffff)+1)&0xffffffff)
        else:stop=0x5a20a0 if interrupt else 0x5a1e30
        if not interrupt or kind=='control-rejected-start':
            for off,v,width in ((0x178,c.get('recent',0x12345678),4),(0x17c,handle,4),(0x12c,handle,4),(0x130,c['mode'],4),(0x134,0,1),(0x13c,c['fade'],4)):
                expect(parent+off+shift,v,width)
            expect(actor+0x1c,1,1);expect(actor+0x24,1,1)
        psentry=0x2a6d10;psstops=[0x115fa0 if interrupt else 0x1164a0]
        psregs=dict(A0=parent,A3=c['fade'],T0=c['interrupt'],S3=handle,S2=c['mode'])
        result.update(originalEntry='004FB620' if ispc else '002A6CF0',interruptPrevious=interrupt)
    elif kind=='control-stop':
        entry=0x4fb2d0;this=parent;args=(handle,);assert first is None
        psentry=0x2a6ce0;psstops=[0x115fa0];psregs=dict(A0=parent,A1=handle)
    elif kind in ('control-fade','actor-fade'):
        entry=0x4fb2f0 if kind=='control-fade' else 0x5a16d0;this=parent if kind=='control-fade' else actor
        fallback=0. if kind=='control-fade' else c['fallback'];args=(handle,bits(c['duration']))+(() if kind=='control-fade' else (bits(fallback),))
        psentry=0x2a6cd0 if kind=='control-fade' else 0x116930;psstops=[p.RETURN] if not ispc else None
        psregs=dict(A0=this,A1=handle,F12=bits(c['duration']),F13=bits(fallback))
        if first is not None:
            a=rows+first*0x60;expect(a+0x20,bits(1/f32(c['duration']) if c['duration']>0 else fallback));expect(a+0x10,3);expect(a+0x58,0);expect(a+0x3c,1,1)
    elif kind in ('actor-find','actor-used'):
        entry=0x5a14c0 if kind=='actor-find' else 0x5a1500;args=(handle,)
        psentry=0x1168d0 if kind=='actor-find' else 0x116870;psstops=[p.RETURN] if not ispc else None;psregs=dict(A0=actor,A1=handle)
        result['expectedValue']=(0 if first is None else rows+first*0x60) if kind=='actor-find' else int(any(key(s['key'])==handle and s.get('uses',0)!=0 for s in slots))
    elif kind=='actor-stop-missing':
        assert first is None;entry=0x5a20a0;args=(handle,c.get('suppress',0))
        psentry=0x115fcc;psstops=[0x116130];psregs=dict(A0=actor,A3=0,S3=handle,S5=c.get('suppress',0))
        result['originalEntry']='005A20A0' if ispc else '00115FA0'
    elif kind=='actor-start':
        selected=None;restart=False
        for i,s in enumerate(slots):
            if key(s['key'])==handle and s.get('uses',0)!=0:selected=i;restart=True
            elif s.get('uses',0)==0 and selected is None:selected=i
        entry=0x5a1e30;args=(request,);stop=None if selected is None else 0x40fa10
        psentry=0x1164c4;psstops=[0x116854 if selected is None else 0x1003c0]
        psregs=dict(A0=actor,A1=request,S2=actor,V0=0,T3=0)
        result.update(originalEntry='005A1E30' if ispc else '001164A0',selectedIndex=selected,restart=restart)
        if selected is not None:
            a=rows+selected*0x60;fade=c['fade'];weight=bits(1. if fade in (0,3) else 0. if fade in (2,4) else .25)
            expect(request+0xc,weight)
            for off,v,width in ((0,animation,4),(4,c.get('mode',3),4),(8,255,1),(0x10,fade,4),(0x20,bits(1/f32(c['fadeOut']) if c['fadeOut']>0 else -.375),4),(0x18,bits(1/f32(c['fadeIn']) if c['fadeIn']>0 else 7.),4),(0x58,bits(f32(c.get('duration',1.))-f32(c['fadeOut'])),4),(0x28,0,4),(0x2c,0xfedcba98,4),(0x4c,1,1),(0x50,((c.get('priority',0x123)<<24)|(c.get('frame',0xfedcba98)&0xffffff))&0xffffffff,4),(0x54,bits(f32(c.get('initial',.5))/f32(c.get('duration',1.))),4),(0x40,int(fade!=2),4),(0x5c,0,4),(0x24,bits(.75),4),(0x30,bits(2.),4)):
                expect(a+off,v,width)
            if not restart:expect(a+0xc,weight)
    else:raise ValueError(kind)
    if ispc:
        p.run(entry,this,args,stop_at=stop)
        result.update(entry=f'{entry:08X}',stop=f'{p.reg("EIP"):08X}',completion='original return' if stop is None else 'real child/event entry;guest discarded',blocks=sum(p.visits.values()))
        value=p.reg('EAX')&0xffffffff
    else:
        for r,v in psregs.items():p.reg(r,v)
        result.update(p.run(psentry,psstops,timeout_us=500000));result['initialRegisters']=psregs;value=p.reg('V0')&0xffffffff
    if kind in ('actor-find','actor-used'):
        if kind=='actor-used' and ispc:value&=255
        assert value==result['expectedValue'];result['returnedValue']=value
    if kind=='actor-start':
        if result['selectedIndex'] is None:
            assert value==0xffffffff;result['returnedValue']=value
        else:
            expected_args=[2,engine+(0x110 if ispc else 0x108),0xffffffff,rows+result['selectedIndex']*0x60,animation]
            observed=[p.uint(p.reg('ESP')+4*i) for i in range(1,6)] if ispc else [p.reg(r)&0xffffffff for r in ('A1','A2','A3','T0','T1')]
            assert observed==expected_args,(observed,expected_args)
            assert (p.reg('ECX') if ispc else p.reg('A0'))==actor;result['eventArguments']=observed
    if kind=='control-start':
        if result['interruptPrevious']:
            observed=[p.uint(p.reg('ESP')+4*i) for i in (1,2)] if ispc else [p.reg(r)&0xffffffff for r in ('A1','A2')]
            assert observed==[c['old'],0]
        else:
            observed=[p.uint(p.reg('ESP')+4)] if ispc else [p.reg('A1')&0xffffffff]
            assert observed==[parent+0x12c+shift]
        assert (p.reg('ECX') if ispc else p.reg('A0'))==actor;result['childArguments']=observed
    if kind=='control-stop' and not ispc:
        assert [p.reg(r)&0xffffffff for r in ('A0','A1','A2')]==[actor,handle,0]
    after=read(area,0x3000)
    assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:18]
    result.update(borrowedStorageBytes=0x3000,beforeSha256=hashlib.sha256(before).hexdigest().upper(),afterSha256=hashlib.sha256(after).hexdigest().upper(),changes=[dict(offset=i,before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b])
    return result


def guest(output,selection):
    output=Path(output).resolve();selection=Path(selection).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    cases=json.loads(selection.read_text())['cases'];assert 0<len(cases)<=100
    started=time.perf_counter();report=dict(kind='original-character-animation-playback-controls',status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selectionSha256=hashlib.sha256(selection.read_bytes()).hexdigest().upper(),cases=[])
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
