#!/usr/bin/env python3
"""Paired original short state hooks with explicit borrowed input observations.

Calls into game consumers are boundaries, never fabricated successful callees.
PS2 SQ/LQ prologues are excluded by declared prefix entries where needed.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from capture_native_ranges import ROOT,PS2,EXPECTED,read_window,read_elf_sections
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    raw=PS2.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2'];sections=read_elf_sections(raw)
    sys.path.insert(0,str(ROOT/'.codex-tmp/emulation-python'))
    import unicorn
    from unicorn import mips_const as reg
    report=dict(kind='paired-original-small-game-flow-hooks',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='DebugMenu,DialogWindow,LoadSave hooks. Literal borrowed global records and input virtual observations only; stop before game consumer entry, no substitute consumer/constructor.',
        limits='Fresh guest per platform/case. PC micro100000/2s;PS2 ordinary integer1000/100ms;30s child. No game startup, EE/MMI, host DLL or OS forwarding.')
    def save():
        report['seconds']=time.perf_counter()-started
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def pc_case(kind,state):
        f=LifetimeFixture();p=f.p;effects=[]
        def alloc(n):
            a=p.allocate(n);p.mu.mem_write(a,b'\xa5'*n);return a
        obj=alloc(0x3c);owner=alloc(0x24);input_obj=alloc(0x10);vt=alloc(0x24);window=alloc(0x280);debug=alloc(0x20)
        p.put_uint(owner+0x1c,input_obj);p.put_uint(input_obj,vt)
        for a,v in ((0x7552a0,owner),(0x765bf4,window),(0x765bf8,debug)):p.put_uint(a,v)
        callbacks=0x34160000;p.mu.mem_map(callbacks,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
        def reset(m):effects.append(dict(slot='reset-observation',receiver=m.reg('ECX')));m.fixture_return()
        def mode(m):effects.append(dict(slot='mode-observation',receiver=m.reg('ECX'),value=m.uint(m.reg('ESP')+4)));m.fixture_return(4)
        p.seams[callbacks]=reset;p.seams[callbacks+0x10]=mode
        p.put_uint(vt+0x20,callbacks);p.put_uint(vt+0x1c,callbacks+0x10)
        entry,stop={'debug-v8':(0x5dd980,0x5ccec0),'debug-v9':(0x5dd9d0,0x5cdb90),
            'dialog-v9':(0x5db290,None),'dialog-v12':(0x5db2c0,0x5c98b0),
            'load-v9':(0x5db140,None),'load-v12':(0x5db160,None)}[kind]
        before=bytes(p.mu.mem_read(window,0x280));p.run(entry,obj,(0,),stop_at=stop)
        result=dict(entry=f'{entry:08X}',stop=f'{p.reg("EIP"):08X}',completion='consumer boundary; guest discarded' if stop else 'original return',
            effects=effects,instructions=sum(p.visits.values()),al=p.reg('EAX')&255,
            borrowed=dict(state=obj,inputOwner=owner,input=input_obj,window=window,debug=debug),thisAfter=p.reg('ECX'))
        if stop:assert p.reg('EIP')==stop
        if kind=='dialog-v9':
            after=bytes(p.mu.mem_read(window,0x280));expected=bytearray(before);expected[0x14:0x26c]=bytes(0x258);assert after==expected
            result.update(windowBefore=before.hex(),windowAfter=after.hex(),clearedBytes=0x258,clearOffset=0x14)
        if kind=='load-v12':assert result['al']==1
        expected_effects=2 if kind in ('debug-v8','load-v12') else 1 if kind in ('debug-v9','load-v9') else 0
        assert len(effects)==expected_effects
        if len(effects)==2:assert effects[1]['value']==7
        assert all(e['receiver']==input_obj for e in effects)
        if kind.startswith('debug'):assert result['thisAfter']==debug
        if kind=='dialog-v12':assert result['thisAfter']==window
        return result
    def ps2_case(kind,state):
        # Prefixes start after SQ prologues and stop before the first original
        # game consumer, or before the unexecuted LQ epilogue. SD/LD-only leaves
        # execute from their real entry with an initialized stack.
        entry,lo,hi,ends={
            'debug-v8':(0x326a2c,0x326a2c,0x326a64,{0x326a64}),
            'debug-v9':(0x3269d0,0x3269d0,0x326a1c,{0x20000000}),
            'dialog-v9':(0x327de0,0x327de0,0x327e28,{0x4076a8}),
            'dialog-v12':(0x327d80,0x327d80,0x327dc0,{0x36c400}),
            'load-v9':(0x344270,0x344270,0x34431c,{0x20000000,0x33a890}),
            'load-v12':(0x3441cc,0x3441cc,0x34421c,{0x21d2f0}),
        }[kind]
        body,offset=read_window('ps2',raw,lo,hi-lo,sections)
        u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN);u.ctl_set_cpu_model(reg.UC_CPU_MIPS64_R4000)
        pages={lo&~4095,(hi-1)&~4095,*(a&~4095 for a in ends),0x49f000,0x21000000,0x22000000,0x23000000}
        for page in pages:u.mem_map(page,4096)
        u.mem_write(lo,body)
        data,stack,callbacks=0x21000000,0x22000800,0x23000000
        obj,owner,input_obj,vt,window,manager,controller=[data+x for x in (0,0x40,0x80,0xc0,0x100,0x400,0xb00)]
        def word(a,v):u.mem_write(a,struct.pack('<I',v))
        word(owner+0x1c,input_obj);word(input_obj,vt);word(vt+0x28,callbacks);word(vt+0x24,callbacks+0x10)
        # Explicit virtual observation functions: jr ra; nop. No game callee body.
        for a in (callbacks,callbacks+0x10):u.mem_write(a,struct.pack('<II',0x03e00008,0))
        gp=0x4a4170
        for off,value in ((0x443c,owner),(0x441c,window),(0x4428,manager),(0x4424,controller)):word(gp-off,value)
        word(manager+0x6a4,state)
        for name,value in (('GP',gp),('SP',stack),('RA',0x20000000),('A0',obj)):
            u.reg_write(getattr(reg,'UC_MIPS_REG_'+name),value)
        tail=[];effects=[];stopped=[]
        def hook(uc,a,n,user):
            tail.append(a)
            if a in ends:stopped.append(a);uc.emu_stop();return
            if a in (callbacks,callbacks+0x10):
                e=dict(slot='reset-observation' if a==callbacks else 'mode-observation',receiver=uc.reg_read(reg.UC_MIPS_REG_A0)&0xffffffff)
                if a==callbacks+0x10:e['value']=uc.reg_read(reg.UC_MIPS_REG_A1)&0xffffffff
                effects.append(e)
            if not(lo<=a<hi or a in (callbacks,callbacks+4,callbacks+0x10,callbacks+0x14)):
                raise RuntimeError(f'Outside audited PS2 prefix {a:X}')
            if a==0x33a890:raise AssertionError('Consumer must be stopped, not executed')
        def intr(uc,n,user):raise RuntimeError(f'No interrupt forwarding {n}')
        u.hook_add(unicorn.UC_HOOK_CODE,hook);u.hook_add(unicorn.UC_HOOK_INTR,intr)
        try:u.emu_start(entry,0x24000000,timeout=100000,count=1000)
        except Exception:
            report['failure']=dict(platform='ps2',kind=kind,state=state,pc=f'{u.reg_read(reg.UC_MIPS_REG_PC):08X}',tail=[f'{a:08X}' for a in tail[-10:]]);raise
        assert stopped,'PS2 limit before declared boundary'
        stop=stopped[0];args=[u.reg_read(getattr(reg,'UC_MIPS_REG_'+n))&0xffffffff for n in ('A0','A1','A2')]
        result=dict(entry=f'{entry:08X}',stop=f'{stop:08X}',completion='original return' if stop==0x20000000 else 'declared prefix boundary; guest discarded',
            effects=effects,argsAtBoundary=args,instructions=len(tail)-1,bytes=body.hex(),fileOffset=offset,
            borrowed=dict(state=obj,inputOwner=owner,input=input_obj,window=window,loadSaveManager=manager,controller=controller))
        expected_effects=2 if kind in ('debug-v8','load-v12') else 1 if kind in ('debug-v9','load-v9') else 0
        assert len(effects)==expected_effects and all(e['receiver']==input_obj for e in effects)
        if len(effects)==2:assert effects[1]['value']==7
        if kind=='dialog-v9':assert args==[window+0x14,0,0x258]
        if kind=='dialog-v12':assert args[0]==window
        if kind=='load-v12':assert args[0]==manager and stop==0x21d2f0
        if kind=='load-v9':
            if state==4:assert stop==0x33a890 and args==[controller,0x36,1]
            else:assert stop==0x20000000
        return result
    cases=[(x,0) for x in ('debug-v8','debug-v9','dialog-v9','dialog-v12','load-v12')]
    cases += [('load-v9',s) for s in (0,3,4,5,0xffffffff)]
    try:
        for kind,state in cases:
            report['pending']=dict(kind=kind,state=state);save()
            a=pc_case(kind,state);report['pending']['pc']=a;save();b=ps2_case(kind,state)
            assert [e['slot'] for e in a['effects']]==[e['slot'] for e in b['effects']]
            report['cases'].append(dict(kind=kind,loadSaveState=state,pc=a,ps2=b));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error));save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(report['cases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(args[1]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
