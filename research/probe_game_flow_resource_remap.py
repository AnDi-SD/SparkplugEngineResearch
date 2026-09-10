#!/usr/bin/env python3
"""Paired original remap decisions and controller state-stack lookup.

PC executes the original full remap including protected entry and getter.
PS2 executes an ordinary integer decision prefix, with explicit state-getter
result and initialized profile input, then discards the guest at its epilogue.
Both actual controller getter leaves also execute independently to return.
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
    getter=read_window('ps2',raw,0x339ba0,0x58,sections)[0]
    remap=read_window('ps2',raw,0x33b430,0x160,sections)[0]
    sys.path.insert(0,str(ROOT/'.codex-tmp/emulation-python'))
    import unicorn
    from unicorn import mips_const as reg
    f=LifetimeFixture();p=f.p
    controller=p.allocate(0x1b0);profile=p.allocate(0x518);records=[p.allocate(0x14) for _ in range(4)]
    p.mu.mem_write(controller,bytes(0x1b0));p.mu.mem_write(profile,bytes(0x518))
    p.put_uint(0x755294,controller);p.put_uint(0x765ad4,profile)
    report=dict(kind='paired-original-game-flow-resource-remap',status='running',remapCases=[],getterCases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),inputs=EXPECTED,
        ps2Code=dict(getter=getter.hex(),remap=remap.hex()),
        pcScope='Original5D6CB0 full decision,5954E0 state getter and initialized profile getter; literal borrowed controller stack/profile fields. No constructor/global startup or engine callee replacement.',
        ps2Scope='Original33B458 decision prefix with explicit S0 getter result and initialized profile; stop before SQ/LQ epilogue and discard. Actual339BA0 getter tested separately to return.',
        limits='PC micro100000/2s per call; PS2 fresh guest1000/100ms per case;30s child. No OS/host DLL execution.')
    def save():
        report.update(seconds=time.perf_counter()-started,lastPc=f'{p.reg("EIP"):08X}',lastPcTail=[f'{a:08X}' for a in p.tail])
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def pcstack(values):
        p.put_uint(controller+0x1ac,(len(values)-1)&0xffffffff)
        for i,v in enumerate(values):p.put_uint(records[i]+0x10,v);p.put_uint(controller+0x15c+4*i,records[i])
    def ps2case(kind,values=(),code=0,state=0,progress=0):
        u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN)
        u.ctl_set_cpu_model(reg.UC_CPU_MIPS64_R4000)
        u.mem_map(0x339000,0x3000,unicorn.UC_PROT_READ|unicorn.UC_PROT_EXEC)
        u.mem_write(0x339ba0,getter);u.mem_write(0x33b430,remap)
        data,ret=0x21000000,0x20000000;u.mem_map(data,4096);u.mem_map(ret,4096,unicorn.UC_PROT_READ|unicorn.UC_PROT_EXEC)
        tail=[];stopped=[]
        if kind=='getter':
            entry=0x339ba0;ends={ret};allowed=lambda a:0x339ba0<=a<0x339bf8
            u.mem_write(data+0x1ac,struct.pack('<I',(len(values)-1)&0xffffffff))
            for i,v in enumerate(values):
                address=data+0x200+0x20*i;u.mem_write(address+0x10,struct.pack('<I',v));u.mem_write(data+0x15c+4*i,struct.pack('<I',address))
            u.reg_write(reg.UC_MIPS_REG_A0,data);u.reg_write(reg.UC_MIPS_REG_RA,ret)
        else:
            entry=0x33b458;ends={0x33b538,0x33b578,0x33b57c};allowed=lambda a:0x33b458<=a<0x33b540 or 0x33b560<=a<0x33b568
            u.mem_map(0x49f000,4096);u.mem_write(0x49fc7c,struct.pack('<I',data));u.mem_write(data+0x514,struct.pack('<I',progress))
            u.reg_write(reg.UC_MIPS_REG_GP,0x4a4170)
            for name,v in (('S0',state),('S1',code)):
                u.reg_write(getattr(reg,'UC_MIPS_REG_'+name),(v if v<0x80000000 else v-(1<<32))&((1<<64)-1))
        def hook(uc,a,n,user):
            tail.append(a)
            if a in ends:stopped.append(a);uc.emu_stop();return
            if not allowed(a):raise RuntimeError(f'Unreviewed PS2 instruction boundary {a:X}')
        def intr(uc,n,user):raise RuntimeError(f'No PS2 interrupt forwarding {n}')
        u.hook_add(unicorn.UC_HOOK_CODE,hook);u.hook_add(unicorn.UC_HOOK_INTR,intr)
        try:u.emu_start(entry,0x22000000,timeout=100000,count=1000)
        except Exception:
            report['ps2Failure']=dict(kind=kind,values=values,code=code,state=state,progress=progress,
                pc=f'{u.reg_read(reg.UC_MIPS_REG_PC):08X}',tail=[f'{a:08X}' for a in tail[-8:]])
            raise
        if not stopped:raise RuntimeError(f'PS2 limit/fault before expected stop: {tail[-8:]}')
        result=dict(value=u.reg_read(reg.UC_MIPS_REG_V0)&0xffffffff,instructions=len(tail)-1,stop=f'{stopped[0]:08X}',
                    completion='ordinary return' if kind=='getter' else 'explicit epilogue boundary; guest discarded')
        del u;return result
    cases=[(24,3,7,53),(24,0,7,24),(44,3,7,52),(44,0,7,44),(49,17,7,55),(49,0,7,49),
           (43,40,6,43),(43,0,6,5),(43,0,7,43),(43,40,7,43),(43,0,0xffffffff,5),(43,0,0x80000000,5),
           (5,39,7,54),(5,39,6,54),(5,0,6,43),(5,0,7,5),(5,0,0xffffffff,43)]
    cases += [(x,3,6,x) for x in (0,1,23,25,42,45,48,50,51,55,56,0xffffffff,0x80000000)]
    try:
        for values,expected in [([],0),([3],3),([3,51,52],3),([51,52],0),([3,17,50],50),([3,0xffffffff,51],0xffffffff),([3,0x80000000,51],0x80000000)]:
            pcstack(values);a=f.call(0x5954e0,controller);b=ps2case('getter',values)
            report['getterCases'].append(dict(stack=values,pc=a,ps2=b,expected=expected));save();assert a==b['value']==expected
        for code,state,progress,expected in cases:
            pcstack([state]);p.put_uint(profile+0x514,progress)
            a=f.call(0x5d6cb0,args=(code,));b=ps2case('remap',code=code,state=state,progress=progress)
            report['remapCases'].append(dict(code=code,state=state,profile514=progress,pc=a,pcInstructions=sum(p.visits.values()),ps2=b,expected=expected))
            save();assert a==b['value']==expected,(code,state,progress,a,b,expected)
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error));save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',remapCases=len(report['remapCases']),getterCases=len(report['getterCases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(args[1]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
