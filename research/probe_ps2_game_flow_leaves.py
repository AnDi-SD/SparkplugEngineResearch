#!/usr/bin/env python3
"""Original PS2 GameFlowState integer leaves only; no EE/MMI/OS or constructor."""
import hashlib,json,struct,sys,time
from pathlib import Path
from pc_instruction_emulator import ROOT,run_bounded
from capture_native_ranges import PS2,EXPECTED,read_window,read_elf_sections


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    output.parent.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();raw=PS2.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2']
    sections=read_elf_sections(raw);entry=0x33c230;size=0xb0
    body,offset=read_window('ps2',raw,entry,size,sections)
    sys.path.insert(0,str(ROOT/'.codex-tmp/emulation-python'))
    import unicorn
    from unicorn import mips_const as reg
    u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN)
    u.ctl_set_cpu_model(reg.UC_CPU_MIPS64_R4000)
    data,ret=0x21000000,0x20000000
    u.mem_map(entry&~4095,4096,unicorn.UC_PROT_READ|unicorn.UC_PROT_EXEC);u.mem_write(entry,body)
    u.mem_map(data,4096);u.mem_map(ret,4096,unicorn.UC_PROT_READ|unicorn.UC_PROT_EXEC)
    report=dict(kind='original-ps2-game-flow-leaves',status='running',sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        executableSha256=EXPECTED['ps2'],entry=entry,bytes=body.hex(),bodySha256=hashlib.sha256(body).hexdigest().upper(),
        fileOffset=offset,cases=[],guest='Unicorn MIPS64 R4000 little endian; original ordinary integer leaves only',
        limits='1000 instructions/100ms per call, 30s child process; no calls, imports, interrupts, EE/MMI or constructor',
        abi='32-bit integer arguments sign extended to 64-bit GPRs; low 32-bit field bits preserved')
    count=[0];tail=[]
    def save():
        report.update(seconds=time.perf_counter()-started,lastPc=u.reg_read(reg.UC_MIPS_REG_PC),lastTail=tail[-8:])
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def code(uc,address,n,user):
        if not entry<=address<entry+size:raise RuntimeError(f'Outside audited leaves: {address:X}')
        count[0]+=1;tail.append(address)
    def intr(uc,n,user):raise RuntimeError(f'No guest interrupt forwarding: {n}')
    u.hook_add(unicorn.UC_HOOK_CODE,code);u.hook_add(unicorn.UC_HOOK_INTR,intr)
    def call(address,args):
        count[0]=0;tail.clear()
        for name,value in zip(('A0','A1','A2','A3'),args):
            signed=value if value<0x80000000 else value-(1<<32)
            u.reg_write(getattr(reg,'UC_MIPS_REG_'+name),signed&((1<<64)-1))
        u.reg_write(reg.UC_MIPS_REG_RA,ret)
        u.emu_start(address,ret,timeout=100000,count=1000)
        assert u.reg_read(reg.UC_MIPS_REG_PC)==ret,'Leaf limit exhausted'
        return u.reg_read(reg.UC_MIPS_REG_V0)
    try:
        u.mem_write(data,b'\xcc'*0x3c)
        for args in ((1,2,3),(0xffffffff,0,0x80000000),(0,0xffffffff,0xffffffff),(0xffffffff,)*3):
            before=bytes(u.mem_read(data,0x3c));call(0x33c230,(data,*args));after=bytes(u.mem_read(data,0x3c))
            expected=bytearray(before)
            for off,value in zip((0x18,0x1c,0x20),args):
                if value!=0xffffffff:struct.pack_into('<I',expected,off,value)
            report['cases'].append(dict(kind='setter',args=args,before=before.hex(),after=after.hex(),instructions=count[0]))
            save();assert after==expected
        u.mem_write(data+0x18,struct.pack('<3I',0x12345678,0x80000000,0xffffffff))
        call(0x33c260,(data,data+0x100,data+0x104,data+0x108))
        actual=bytes(u.mem_read(data+0x100,12));assert actual==bytes(u.mem_read(data+0x18,12))
        report['cases'].append(dict(kind='getter',output=actual.hex(),instructions=count[0]));save()
        for address in range(0x33c280,0x33c2e0,0x10):
            before=bytes(u.mem_read(data,0x3c));value=call(address,(data,0,0,0))
            assert bytes(u.mem_read(data,0x3c))==before
            if address in (0x33c280,0x33c2d0):assert value==1
            report['cases'].append(dict(kind='base-hook',address=address,v0=value,instructions=count[0],returnClaim='true' if address in (0x33c280,0x33c2d0) else 'void; V0 ignored'));save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error));save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',cases=len(report['cases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(args[1]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
