#!/usr/bin/env python3
"""Run original ordinary-FPU PS2 direction prefix to an explicit branch boundary.

Each finite float32 case owns a fresh guest; the guest is discarded after the
observed branch. No surrounding SQ/LQ, widget/CRT call, or full frame executes.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from capture_native_ranges import PS2,EXPECTED,read_window,read_elf_sections


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    output.parent.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();raw=PS2.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2']
    entry,size=0x33b9d4,0x138;body,fileoff=read_window('ps2',raw,entry,size,read_elf_sections(raw))
    sys.path.insert(0,str(ROOT/'.codex-tmp/emulation-python'))
    import unicorn
    from unicorn import mips_const as reg
    boundaries={0x33bcb4:0,0x33be60:1,0x33c008:2,0x33bb0c:3,0x33b9a0:None}
    inputs=(0,0x3f490fda,0x3f490fdb,0x3f490fdc,0xbf490fda,0xbf490fdb,0xbf490fdc,
            0x4016cbe3,0x4016cbe4,0x4016cbe5,0xc016cbe3,0xc016cbe4,0xc016cbe5,0x40490fdb,0xc0490fdb)
    report=dict(kind='original-ps2-game-flow-direction-prefix',status='running',cases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),executableSha256=EXPECTED['ps2'],
        entry=entry,byteSize=size,bytes=body.hex(),fileOffset=fileoff,
        scope='Original ordinary-FPU branch prefix on finite normal float32/zero only; explicit branch stop, no full frame or EE/MMI execution',
        limits='Fresh guest for each case;1000 instructions/100ms;30s child; no resumption after boundary/limit/fault',
        guest='Unicorn MIPS64 R4000 little endian with explicit CP0 FPU enable',boundaries=boundaries)
    def save():
        report['seconds']=time.perf_counter()-started
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for bits in inputs:
            u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN)
            u.ctl_set_cpu_model(reg.UC_CPU_MIPS64_R4000)
            u.reg_write(reg.UC_MIPS_REG_CP0_STATUS,u.reg_read(reg.UC_MIPS_REG_CP0_STATUS)|(1<<29))
            u.mem_map(0x33b000,0x2000,unicorn.UC_PROT_READ|unicorn.UC_PROT_EXEC);u.mem_write(entry,body)
            data=0x21000000;u.mem_map(data,4096);u.mem_write(data+0x30,struct.pack('<I',data+0x100))
            u.mem_write(data+0x108,struct.pack('<I',bits));u.reg_write(reg.UC_MIPS_REG_S4,data)
            tail=[];stopped=[]
            def code(uc,address,n,user):
                tail.append(address)
                if address in boundaries:stopped.append(address);uc.emu_stop();return
                if not entry<=address<entry+size:raise RuntimeError(f'Outside audited prefix: {address:X}')
            def intr(uc,n,user):raise RuntimeError(f'No guest interrupt forwarding: {n}')
            u.hook_add(unicorn.UC_HOOK_CODE,code);u.hook_add(unicorn.UC_HOOK_INTR,intr)
            u.emu_start(entry,0x33d000,timeout=100000,count=1000)
            if not stopped:raise RuntimeError(f'Prefix failed to reach an explicit branch boundary: {tail[-8:]}')
            chosen=boundaries[stopped[0]]
            angle=struct.unpack('<f',struct.pack('<I',bits))[0]
            q=struct.unpack('<f',bytes.fromhex('db0f493f'))[0];three=struct.unpack('<f',bytes.fromhex('e4cb1640'))[0]
            expected=0 if -q<=angle<=q else 1 if angle<=-three or angle>=three else 2 if angle<0 else 3
            report['cases'].append(dict(angleBits=f'{bits:08X}',direction=chosen,boundary=f'{stopped[0]:08X}',
                originalInstructionCount=len(tail)-1,tail=[f'{x:08X}' for x in tail[-8:]],reason='explicit branch boundary; guest discarded'))
            save();assert chosen==expected,(hex(bits),chosen,expected)
            del u
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error));save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',cases=len(report['cases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(args[1]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
