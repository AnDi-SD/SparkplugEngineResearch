#!/usr/bin/env python3
"""Paired original AIAction leaf contracts and notification dispatch order.

Borrowed records use original base leaf/notification code, with observation
hooks only. PS2 notification stops before its excluded SQ/LQ epilogue.
"""
from pathlib import Path
import hashlib,json,struct,sys,time,traceback
from capture_native_ranges import ROOT,PS2,EXPECTED,read_window,read_elf_sections
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture


def guest(output,selection='all'):
    if selection not in ('all','remaining-leaves'):raise ValueError('Explicit all or remaining-leaves required')
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    raw=PS2.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2'];sections=read_elf_sections(raw)
    sys.path.insert(0,str(ROOT/'.codex-tmp/emulation-python'))
    import unicorn
    from unicorn import mips_const as reg
    report=dict(kind='paired-original-ai-action-base-leaves',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selection=selection,
        scope='Literal borrowed AIAction/message/base-object records; original true/no-op/false/clear leaves and notification bodies. Hooks observe original calls; no substituted game callee.',
        limits='Fresh guest per platform/case;PC micro100000/2s;PS2 ordinary integer1000/100ms;30s child. PS2 notify starts223414 withV1=28 and stops before epilogue; no EE/MMI/OS.')
    def save():
        report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def pc(kind,code,child):
        f=LifetimeFixture();p=f.p
        obj=p.allocate(0x3a8);receiver=p.allocate(0x10);vt=p.allocate(8);msg=p.allocate(0x10)
        p.mu.mem_write(obj,b'\xa5'*0x3a8);p.put_uint(obj,0x702450);p.put_uint(obj+0x10,receiver if child else 0)
        p.put_uint(receiver,vt);p.put_uint(vt+4,0x5b7a00);p.put_uint(msg,code)
        effects=[]
        def observe(u,a,n,user):
            if kind=='notify' and a==0x4f3df0 and p.reg('ECX')==obj:effects.append('self-v9')
            if a==0x5b7a00 and p.reg('ECX')==receiver:
                assert p.uint(p.reg('ESP')+4)==msg;effects.append('forward-original-message')
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
        entry,args={'notify':(0x58ea60,(msg,)),'clear':(0x58ea50,()),'true':(0x4f3df0,()),'true13':(0x5a7db0,(0,)),
            'void10':(0x5b7a00,(0,)),'void12':(0x48eaa0,()),'false14':(0x518dc0,(0,))}[kind]
        before=bytes(p.mu.mem_read(obj,0x3a8));value=f.call(entry,obj,args);after=bytes(p.mu.mem_read(obj,0x3a8))
        expected=bytearray(before)
        if kind=='clear':expected[0x374:0x378]=bytes(4)
        assert after==expected
        return dict(entry=f'{entry:08X}',completion='original return',effects=effects,al=value&255,instructions=sum(p.visits.values()),
            changedOffsets=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b])
    def ps2(kind,code,child):
        u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN);u.ctl_set_cpu_model(reg.UC_CPU_MIPS64_R4000)
        code_ranges=[(0x223230,0x20),(0x223400,0xe0),(0x100810,0x10),(0x492060,0x4c)]
        for page in (0x223000,0x100000,0x492000,0x21000000,0x20000000):u.mem_map(page,4096)
        for address,n in code_ranges:u.mem_write(address,read_window('ps2',raw,address,n,sections)[0])
        obj,receiver,vt,msg=0x21000000,0x21000400,0x21000420,0x21000440
        def word(a,v):u.mem_write(a,struct.pack('<I',v))
        u.mem_write(obj,b'\xa5'*0x3ac);word(obj,0x492060);word(obj+0x10,receiver if child else 0);word(receiver,vt);word(vt+0xc,0x100810);word(msg,code)
        entry={'notify':0x223414,'clear':0x223490,'true':0x2234c0,'true13':0x223480,'void10':0x2234a0,'void12':0x223240,'false14':0x223230}[kind]
        ends={0x223440,0x223460} if kind=='notify' else {0x20000000}
        for name,value in (('A0',obj),('A1',msg if kind=='notify' else 0),('V1',28),('RA',0x20000000)):
            u.reg_write(getattr(reg,'UC_MIPS_REG_'+name),value)
        effects=[];tail=[];stopped=[]
        def observe(uc,a,n,user):
            tail.append(a)
            if a in ends:stopped.append(a);uc.emu_stop();return
            if kind=='notify' and a==0x2234d0 and uc.reg_read(reg.UC_MIPS_REG_A0)==obj:effects.append('self-v9')
            if a==0x100810:
                assert uc.reg_read(reg.UC_MIPS_REG_A0)==receiver and uc.reg_read(reg.UC_MIPS_REG_A1)==msg;effects.append('forward-original-message')
            if not(0x223230<=a<0x223250 or 0x223414<=a<0x223460 or 0x223480<=a<0x2234e0 or 0x100810<=a<0x100818):
                raise RuntimeError(f'Outside ordinary audited instructions {a:X}')
        def interrupt(uc,n,user):raise RuntimeError(f'No guest interrupt forwarding {n}')
        u.hook_add(unicorn.UC_HOOK_CODE,observe);u.hook_add(unicorn.UC_HOOK_INTR,interrupt)
        before=bytes(u.mem_read(obj,0x3ac));u.emu_start(entry,0x22000000,timeout=100000,count=1000);assert stopped
        after=bytes(u.mem_read(obj,0x3ac));expected=bytearray(before)
        if kind=='clear':expected[0x378:0x37c]=bytes(4)
        assert after==expected
        return dict(entry=f'{entry:08X}',completion='notification prefix boundary; guest discarded' if kind=='notify' else 'original return',
            stop=f'{stopped[0]:08X}',effects=effects,v0=u.reg_read(reg.UC_MIPS_REG_V0)&0xffffffff,instructions=len(tail)-1,
            changedOffsets=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b])
    cases=[('notify',c,child) for c in (0,27,28,29,0xffffffff) for child in (False,True)]
    cases += [(kind,0,False) for kind in ('clear','true','true13','void10','void12','false14')]
    if selection=='remaining-leaves':cases=[c for c in cases if c[0] not in ('notify','clear')]
    try:
        for kind,code,child in cases:
            report['pending']=dict(kind=kind,code=code,child=child);save()
            a=pc(kind,code,child);report['pending']['pc']=a;save();b=ps2(kind,code,child)
            assert a['effects']==b['effects']
            if kind=='notify':assert a['effects']==(['self-v9'] if code==28 else [])+(['forward-original-message'] if child else [])
            if kind in ('true','true13'):assert a['al']==b['v0']==1
            if kind=='false14':assert a['al']==b['v0']==0
            report['cases'].append(dict(kind=kind,messageCode=code,childPresent=child,pc=a,ps2=b));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),errorType=type(error).__name__,traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(report['cases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
