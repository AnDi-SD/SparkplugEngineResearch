#!/usr/bin/env python3
"""Original PC machine protocol with paired bounded PS2 scalar prefixes.

Literal machine/state interface records select existing original boolean
leaves. Original message construction runs on PC with no observers; PS2
switch/pop stop at its real consumer entry. No substituted message consumer.
"""
from pathlib import Path
import hashlib,json,struct,sys,time,traceback
from capture_native_ranges import ROOT,PS2,EXPECTED,read_window,read_elf_sections
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture

FIELDS=dict(current=0x130,previous=0x134,gate=0x138,previousData=0x13c,data=0x140,nextData=0x144,depth=0x218,mode=0x21c)
CODE_ENTER_TRUE=0x5a7db0;CODE_ENTER_FALSE=0x5a7dc0


def guest(output,selection='all'):
    if selection not in ('all','filter-only'):raise ValueError('Explicit all or filter-only selection required')
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local report required')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    raw=PS2.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2'];sections=read_elf_sections(raw)
    sys.path.insert(0,str(ROOT/'.codex-tmp/emulation-python'))
    import unicorn
    from unicorn import mips_const as reg
    report=dict(kind='paired-original-character-machine-protocol',status='running',cases=[],inputs=EXPECTED,selection=selection,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Borrowed machine and two state-interface records, original constant true/false enter leaves, original base suspend/resume/notify leaves. No game class initialization or active character claim. PC original message code consumer runs with observer pointer null. PS2 switches/pop stop at100520;other prefixes exclude SQ/LQ frames.',
        limits='Fresh guest per case/platform. PC100k/2s,PS2 ordinary2000/100ms,30s process. No memory/OS forwarding, unknown game callback success or stopped guest resumption.')
    def save():
        report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def logical(read,obj,delta):
        return {**{k:read(obj+v+delta) for k,v in FIELDS.items()},'lastFlag':read(obj+0x220+delta)&255}
    def values(case):return dict(current=2,previous=9,gate=0x99,previousData=0xabcdef01,data=0x13572468,nextData=0x24681357,depth=case.get('depth',2),mode=0x87654321)
    def pc(case):
        f=LifetimeFixture();p=f.p;obj=p.allocate(0x224);states=[p.allocate(0x3c) for _ in range(2)];vt=p.allocate(68);msg=p.allocate(0x20)
        p.mu.mem_write(obj,b'\xa5'*0x224);p.put_uint(obj,0x6f6680);p.put_uint(obj+4,0)
        p.mu.mem_write(vt,bytes(p.mu.mem_read(0x6f81b0,68)));p.put_uint(vt+0x1c,CODE_ENTER_TRUE if case.get('accept',True) else CODE_ENTER_FALSE)
        for state in states:
            p.mu.mem_write(state,b'\xa5'*0x3c);p.put_uint(state,vt);p.put_uint(state+0x24,0);p.mu.mem_write(state+0x1c,bytes(3))
        p.put_uint(obj+0x148,states[0]);p.put_uint(obj+0x150,states[1] if case.get('child',True) else 0)
        for k,v in values(case).items():p.put_uint(obj+FIELDS[k],v)
        for i in range(5):p.put_uint(obj+0x1f0+i*4,0);p.put_uint(obj+0x204+i*4,0x11111111*(i+1))
        p.put_uint(msg,case.get('code',0));events=[]
        if case['kind']=='filter':
            owner=p.allocate(16);p.mu.mem_write(owner,b'\xa5'*16);p.put_uint(owner+12,case['code']);p.put_uint(obj+0x24,owner)
        def observe(u,a,n,user):
            receiver=p.reg('ECX');arg=p.uint(p.reg('ESP')+4)
            if a==0x40f9a0:
                assert receiver==obj
                events.append(dict(kind='event',code=arg,state=p.uint(p.reg('ESP')+8),data=p.uint(p.reg('ESP')+12),machine=logical(p.uint,obj,0)))
            if a in (CODE_ENTER_TRUE,CODE_ENTER_FALSE):
                if receiver==obj:events.append(dict(kind='select',argument=arg))
                elif receiver in states:
                    assert arg==obj+0x140;events.append(dict(kind='enter',state=states.index(receiver)*2,accept=case.get('accept',True)))
            if a in (0x513090,0x5130f0) and receiver in states:
                assert arg==obj+(0x144 if a==0x513090 else 0x140)
                events.append(dict(kind='suspend' if a==0x513090 else 'resume',state=states.index(receiver)*2))
            if a==0x5b7a00 and receiver in states:
                events.append(dict(kind='forward-message' if arg==msg else 'state-v12',state=states.index(receiver)*2))
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
        kind=case['kind'];entry={'mode':0x4fac50,'enter':0x4faba0,'reset':0x4fac00,'switch':0x4fad70,'push':0x4faf30,'pop':0x4face0,'notify':0x4fae50,'filter':0x4f5000}[kind]
        args=(case['code'],0xaabbccdd) if kind=='mode' else (msg,) if kind=='notify' else (0xdeadbeef,) if kind=='filter' else ()
        stop={28:0x4fadf0,30:0x4fb040}.get(case.get('code')) if kind=='notify' else None
        before=bytes(p.mu.mem_read(obj,0x224));state_before=[bytes(p.mu.mem_read(s,0x3c)) for s in states]
        p.run(entry,obj,args,stop_at=stop)
        result=dict(entry=f'{entry:08X}',stop=f'{p.reg("EIP"):08X}',completion='original virtual consumer entry;guest discarded' if stop else 'original return',
            instructions=sum(p.visits.values()),before=logical(lambda a:int.from_bytes(before[a-obj:a-obj+4],'little'),obj,0),after=logical(p.uint,obj,0),events=events,
            changedOffsets=[i for i,(a,b) in enumerate(zip(before,bytes(p.mu.mem_read(obj,0x224)))) if a!=b],
            stackStates=[p.uint(obj+0x1f0+i*4) for i in range(5)],stackData=[p.uint(obj+0x204+i*4) for i in range(5)],
            stateFlagTriples=[bytes(p.mu.mem_read(s+0x1c,3)).hex() for s in states])
        if kind=='filter':result['boolean']=p.reg('EAX')&255;assert result['boolean']==int(case['code']&0x18==0)
        expected=bytearray(before)
        def put(off,v):expected[off:off+4]=struct.pack('<I',v&0xffffffff)
        initial=values(case)
        if kind=='mode' and case['code'] in (0,1,3):put(0x21c,{0:0,1:5,3:6}[case['code']])
        elif kind=='enter' and case.get('accept',True):put(0x138,0)
        elif kind=='reset' or kind=='notify' and case['code']==0x27f3:
            for off in (0x130,0x134,0x13c,0x140,0x144,0x218,0x21c):put(off,0)
            put(0x138,1);expected[0x220]=1
        elif kind in ('switch','push'):
            for off,v in ((0x134,2),(0x130,0),(0x13c,initial['data']),(0x140,initial['nextData']),(0x138,0)):put(off,v)
            if kind=='push':
                depth=initial['depth'];put(0x1f0+depth*4,2);put(0x204+depth*4,initial['data']);put(0x218,depth+1)
        elif kind=='pop':
            depth=initial['depth']-1
            for off,v in ((0x218,depth),(0x134,2),(0x130,0),(0x13c,initial['data']),(0x140,0x11111111*(depth+1)),(0x138,0)):put(off,v)
        assert bytes(p.mu.mem_read(obj,0x224))==expected,(case,result)
        for i,state in enumerate(states):
            desired=bytearray(state_before[i])
            if kind in ('switch','push') and i==0:desired[0x1c:0x1f]=b'\1'*3
            assert bytes(p.mu.mem_read(state,0x3c))==desired
        if kind=='push':assert [e['code'] for e in events if e['kind']=='event']==[0x2722,0x2720]
        if kind=='switch':assert [e['code'] for e in events if e['kind']=='event']==[0x2720]
        if kind=='pop':assert [e['code'] for e in events if e['kind']=='event']==[0x2723]
        return result
    def ps2(case):
        kind=case['kind'];u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN);u.ctl_set_cpu_model(reg.UC_CPU_MIPS64_R4000)
        for page in (0x2b3000,0x2b4000,0x2c8000,0x100000,0x21000000,0x22000000,0x20000000):u.mem_map(page,4096)
        for a,n in ((0x2b3d20,0x3e0),(0x2c8df0,0x50),(0x2b47f0,0x20)):u.mem_write(a,read_window('ps2',raw,a,n,sections)[0])
        obj,states,vt,msg,machine_vt=0x21000000,[0x21000300,0x21000340],0x21000380,0x210003d0,0x21000400
        def word(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
        def read(a):return int.from_bytes(u.mem_read(a,4),'little')
        u.mem_write(obj,b'\xa5'*0x230);word(obj,machine_vt);word(obj+4,0)
        word(machine_vt+0x4c,0x2b3f00);word(machine_vt+0x44,0x2b4100);word(machine_vt+0x54,0x2b3df0)
        word(vt+0x24,0x2c8e00 if case.get('accept',True) else 0x2c8df0);word(vt+0xc,0x2c8e20)
        for state in states:u.mem_write(state,b'\xa5'*0x3c);word(state,vt);word(state+0x24,0)
        word(obj+0x154,states[0]);word(obj+0x15c,states[1] if case.get('child',True) else 0)
        for k,v in values(case).items():word(obj+FIELDS[k]+12,v)
        for i in range(5):word(obj+0x1fc+i*4,0);word(obj+0x210+i*4,0x11111111*(i+1))
        word(msg,case.get('code',0))
        if kind=='filter':
            owner=0x21000500;u.mem_write(owner,b'\xa5'*16);word(owner+12,case['code']);word(obj+0x24,owner)
        entry,end={'mode':(0x2b3d20,0x20000000),'enter':(0x2b40bc,0x2b40e8),'reset':(0x2b3d90,0x2b3ddc),
            'switch':(0x2b3f7c,0x100520),'pop':(0x2b401c,0x100520),'notify':(0x2b3e20,0x2b3ee8),'filter':(0x2b47f0,0x20000000)}[kind]
        if kind=='notify':end={28:0x2b3f00,30:0x2b4100}.get(case['code'],end)
        for n,v in (('A0',obj),('A1',case['code'] if kind=='mode' else msg if kind=='notify' else 0xdeadbeef if kind=='filter' else 0),('A2',0xaabbccdd),('V0',1),('V1',0x27f3),('SP',0x22000800),('RA',0x20000000)):
            u.reg_write(getattr(reg,'UC_MIPS_REG_'+n),v)
        events=[];tail=[];stopped=[]
        def observe(uc,a,n,user):
            tail.append(a)
            if a==end:stopped.append(a);uc.emu_stop();return
            receiver=uc.reg_read(reg.UC_MIPS_REG_A0);argument=uc.reg_read(reg.UC_MIPS_REG_A1)
            if a in (0x2c8e00,0x2c8df0):
                assert receiver in states and argument==obj+0x14c;events.append(dict(kind='enter',state=states.index(receiver)*2,accept=case.get('accept',True)))
            if a==0x2b3df0:assert receiver==obj;events.append(dict(kind='select',argument=argument))
            if a==0x2c8e20:assert receiver in states and argument==msg;events.append(dict(kind='forward-message',state=states.index(receiver)*2))
            if not(0x2b3d20<=a<0x2b4100 or 0x2c8df0<=a<0x2c8e40 or 0x2b47f0<=a<0x2b4810):raise RuntimeError(f'Outside audited ordinary prefix {a:X}')
            w=read(a)
            if w>>26 in (0x1e,0x1f,0x12,0x1c):raise RuntimeError(f'Excluded R5900 instruction {a:X}')
        def intr(uc,n,user):raise RuntimeError(f'No interrupt forwarding {n}')
        u.hook_add(unicorn.UC_HOOK_CODE,observe);u.hook_add(unicorn.UC_HOOK_INTR,intr)
        before=bytes(u.mem_read(obj,0x230));u.emu_start(entry,0x23000000,timeout=100000,count=2000);assert stopped
        result=dict(entry=f'{entry:08X}',stop=f'{end:08X}',completion='original return' if end==0x20000000 else 'declared prefix boundary;guest discarded',
            instructions=len(tail)-1,after=logical(read,obj,12),events=events,
            changedOffsets=[i for i,(a,b) in enumerate(zip(before,bytes(u.mem_read(obj,0x230)))) if a!=b])
        if kind=='filter':result['boolean']=u.reg_read(reg.UC_MIPS_REG_V0);assert result['boolean']==int(case['code']&0x18==0)
        if end==0x100520:
            assert u.reg_read(reg.UC_MIPS_REG_A0)==obj
            result['eventAtBoundary']=dict(code=u.reg_read(reg.UC_MIPS_REG_A1),state=u.reg_read(reg.UC_MIPS_REG_A2),data=u.reg_read(reg.UC_MIPS_REG_A3),machine=result['after'])
        return result
    cases=[dict(kind='mode',code=c) for c in (0,1,2,3,4,0x80000000,0xffffffff)]
    cases += [dict(kind='enter',accept=b) for b in (False,True)]+[dict(kind='reset'),dict(kind='switch')]
    cases += [dict(kind='pop',depth=d) for d in (1,5)]+[dict(kind='push',depth=d) for d in (0,4)]
    cases += [dict(kind='notify',code=c,child=b) for c,b in [(28,True),(30,True),(0x27f3,True)]+[(c,b) for c in (0,27,29,0xffffffff) for b in (False,True)]]
    cases += [dict(kind='filter',code=c) for c in (0,7,8,16,24,32,0x100,0x80000000,0xffffffff)]
    if selection=='filter-only':cases=[c for c in cases if c['kind']=='filter']
    try:
        for case in cases:
            report['pending']=case.copy();save();a=pc(case);report['pending']['pc']=a;save();b=None
            if case['kind']!='push':
                b=ps2(case)
                if case['kind'] in ('switch','pop'):
                    event=next(e for e in a['events'] if e['kind']=='event');expected={k:event[k] for k in ('code','state','data','machine')};assert b['eventAtBoundary']==expected,(case,b,expected)
                else:assert a['after']==b['after'] and a['events']==b['events'],(case,a,b)
            report['cases'].append(dict(input=case,pc=a,ps2=b));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pcCases=len(cases),pairedCases=sum(c['ps2'] is not None for c in report['cases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
