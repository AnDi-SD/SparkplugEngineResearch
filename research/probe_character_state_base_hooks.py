#!/usr/bin/env python3
"""Paired original CharacterState dispatch prefixes and reset/constant leaves.

Borrowed records and original boolean leaves select predicate branches. Unknown
consumers are hard stop boundaries; no successful consumer body is substituted.
"""
from pathlib import Path
import hashlib,json,struct,sys,time,traceback
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
    report=dict(kind='paired-original-character-state-base-hooks',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Borrowed base state/predicate/consumer records. Original constant predicate and self-v12 leaves. Nonzero pending handle stops before release consumer; zero handle returns on PC and reaches pre-epilogue boundary on PS2. Reset/leaves execute fully.',
        limits='Fresh guest per platform/case. PC micro100000/2s;PS2 ordinary1000/100ms;30s child. SQ/LQ excluded, no EE/MMI/OS or substitute game consumer.')
    def save():
        report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def expected_reset(before,zero):
        b=bytearray(before);b[0x1c:0x1f]=b'\x01'*3;b[0x24:0x2c]=bytes(8);b[0x2c]=0;b[0x30:0x3c]=zero;return bytes(b)
    def pc(kind,pending,predicate):
        f=LifetimeFixture();p=f.p;obj=p.allocate(0x3c);owner=p.allocate(0x10);vt=p.allocate(0x40);consumer=p.allocate(0x10)
        p.mu.mem_write(obj,b'\xa5'*0x3c);p.put_uint(obj,0x6f81b0);p.put_uint(obj+0x14,owner);p.put_uint(obj+0x18,consumer)
        p.put_uint(obj+0x24,pending);p.put_uint(owner,vt);p.put_uint(vt+0x38,0x4f3df0 if predicate else 0x4a1bf0)
        effects=[]
        def observe(u,a,n,user):
            if kind.startswith('v') and a in (0x4f3df0,0x4a1bf0) and p.reg('ECX')==owner:effects.append('predicate')
            if kind in ('v7','v11') and a==0x5b7a00 and p.reg('ECX')==obj:
                assert p.uint(p.reg('ESP')+4)==0x13572468;effects.append('self-v12')
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
        entry={'v7':0x512fa0,'v8':0x512ff0,'v9':0x513040,'v10':0x513090,'v11':0x5130f0,
               'reset':0x513140,'true13':0x5a7db0,'false14':0x5a7dc0,'void12':0x5b7a00,'void15':0x5b7a00}[kind]
        stop=(0x4fb2f0 if predicate else 0x4fb2d0) if pending and kind.startswith('v') else None
        args=() if kind in ('v9','reset') else (0x13572468,)
        before=bytes(p.mu.mem_read(obj,0x3c));p.run(entry,obj,args,stop_at=stop);after=bytes(p.mu.mem_read(obj,0x3c))
        assert after==(expected_reset(before,bytes(p.mu.mem_read(0x7600e0,12))) if kind=='reset' else before)
        result=dict(entry=f'{entry:08X}',stop=f'{p.reg("EIP"):08X}',completion='consumer boundary; guest discarded' if stop else 'original return',
            effects=effects,al=p.reg('EAX')&255,instructions=sum(p.visits.values()),before=before.hex(),after=after.hex())
        if stop:
            assert p.reg('EIP')==stop and p.reg('ECX')==consumer and p.uint(p.reg('ESP')+4)==pending
            result['consumerArgument']=p.uint(p.reg('ESP')+4)
            if predicate:result['fadeBits']=p.uint(p.reg('ESP')+8);assert result['fadeBits']==0x3ecccccd
        return result
    def ps2(kind,pending,predicate):
        entries={'v7':(0x2c9060,0x2c90d4),'v8':(0x2c8fdc,0x2c9040),'v9':(0x2c8f5c,0x2c8f68),
            'v10':(0x2c8edc,0x2c8f40),'v11':(0x2c8e50,0x2c8ebc),'reset':(0x2c8ac0,0x20000000),
            'true13':(0x2c8e00,0x20000000),'false14':(0x2c8df0,0x20000000),'void12':(0x2c8e30,0x20000000),'void15':(0x2c8e10,0x20000000)}
        entry,end=entries[kind];stop=(0x2a6cd0 if predicate else 0x2a6ce0) if pending and kind.startswith('v') else end
        u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN);u.ctl_set_cpu_model(reg.UC_CPU_MIPS64_R4000)
        for page in (0x2c8000,0x2c9000,0x2a6000,0x476000,0x49a000,0x21000000,0x22000000,0x20000000):u.mem_map(page,4096)
        for a,n in ((0x2c8ac0,0x640),(0x49a9e0,76),(0x476f50,12)):u.mem_write(a,read_window('ps2',raw,a,n,sections)[0])
        obj,owner,vt,consumer=0x21000000,0x21000080,0x210000c0,0x21000140
        def word(a,v):u.mem_write(a,struct.pack('<I',v))
        u.mem_write(obj,b'\xa5'*0x3c);word(obj,0x49a9e0);word(obj+0x14,owner);word(obj+0x18,consumer);word(obj+0x24,pending)
        word(owner,vt);word(vt+0x40,0x2c8e00 if predicate else 0x2c8df0)
        for n,v in (('A0',obj),('A1',0x13572468),('SP',0x22000800),('RA',0x20000000)):u.reg_write(getattr(reg,'UC_MIPS_REG_'+n),v)
        effects=[];tail=[];stopped=[]
        def observe(uc,a,n,user):
            tail.append(a)
            if a==stop:stopped.append(a);uc.emu_stop();return
            if a in (0x2c8e00,0x2c8df0) and uc.reg_read(reg.UC_MIPS_REG_A0)==owner:effects.append('predicate')
            if a==0x2c8e30 and kind!='void12':
                assert uc.reg_read(reg.UC_MIPS_REG_A0)==obj and uc.reg_read(reg.UC_MIPS_REG_A1)==0x13572468;effects.append('self-v12')
            if not(0x2c8ac0<=a<=0x2c8b00 or 0x2c8df0<=a<=0x2c90d0):raise RuntimeError(f'Outside audited ordinary prefix {a:X}')
            w=int.from_bytes(uc.mem_read(a,4),'little')
            if w>>26 in (0x1e,0x1f,0x12,0x1c):raise RuntimeError(f'Excluded R5900 instruction {a:X}')
        def intr(uc,n,user):raise RuntimeError(f'No interrupt forwarding {n}')
        u.hook_add(unicorn.UC_HOOK_CODE,observe);u.hook_add(unicorn.UC_HOOK_INTR,intr)
        before=bytes(u.mem_read(obj,0x3c));u.emu_start(entry,0x23000000,timeout=100000,count=1000);assert stopped
        after=bytes(u.mem_read(obj,0x3c));assert after==(expected_reset(before,read_window('ps2',raw,0x476f50,12,sections)[0]) if kind=='reset' else before)
        result=dict(entry=f'{entry:08X}',stop=f'{stop:08X}',completion='original return' if stop==0x20000000 else 'declared prefix boundary; guest discarded',
            effects=effects,v0=u.reg_read(reg.UC_MIPS_REG_V0)&0xffffffff,instructions=len(tail)-1,before=before.hex(),after=after.hex())
        if pending and kind.startswith('v'):
            assert u.reg_read(reg.UC_MIPS_REG_A0)==consumer and u.reg_read(reg.UC_MIPS_REG_A1)==pending
            result['consumerArgument']=pending
            if predicate:result['fadeBits']=u.reg_read(reg.UC_MIPS_REG_F12)&0xffffffff;assert result['fadeBits']==0x3ecccccd
        return result
    cases=[(f'v{i}',p,b) for i in range(7,12) for p,b in ((0,False),(0x24681357,False),(0x24681357,True))]
    cases += [(k,0,False) for k in ('reset','true13','false14','void12','void15')]
    try:
        for kind,pending,predicate in cases:
            report['pending']=dict(kind=kind,handle=pending,predicate=predicate);save();a=pc(kind,pending,predicate);report['pending']['pc']=a;save();b=ps2(kind,pending,predicate)
            expected=['predicate'] if pending else ['self-v12'] if kind in ('v7','v11') else []
            assert a['effects']==b['effects']==expected
            if not pending and kind in ('v7','v8','true13'):assert a['al']==b['v0']==1
            if kind=='false14':assert a['al']==b['v0']==0
            report['cases'].append(dict(kind=kind,handle=pending,predicate=predicate,pc=a,ps2=b));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),errorType=type(error).__name__,traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(report['cases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
