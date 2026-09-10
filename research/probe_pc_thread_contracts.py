#!/usr/bin/env python3
"""Original spPCThread decisions under explicit Kernel32 return-value inputs."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED,PC,pefile
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local report required')
    started=time.perf_counter();report=dict(kind='original-pc-thread-platform-decisions',status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=[],scope='Actual factory and methods with declared Kernel32 result inputs; no host thread,callback execution,scheduling or PS2 counterpart. Handle is literal borrowed input and cleared before original cold teardown.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    imports={0x6d9064:('CreateThread',6),0x6d9074:('WaitForSingleObject',2),0x6d905c:('GetExitCodeThread',2),0x6d9058:('ResumeThread',1),0x6d9060:('SuspendThread',1),0x6d9170:('TerminateThread',2)}
    pe=pefile.PE(str(PC));actual={a.address:(lib.dll.decode(),a.name.decode()) for lib in pe.DIRECTORY_ENTRY_IMPORT for a in lib.imports if a.name}
    assert all(actual[a]==('KERNEL32.dll',name) for a,(name,_) in imports.items())
    cases=[dict(kind='create',result=r,handle=0) for r in (0,0x12345678)]
    cases += [dict(kind='wait',result=r,handle=h) for h,r in ((0,0),(0x12345678,0),(0x12345678,0x102),(0x12345678,0x80),(0x12345678,0xffffffff))]
    cases += [dict(kind='running',result=1,handle=h,exitCode=e) for h,e in ((0,0),(0x12345678,0x103),(0x12345678,0),(0x12345678,0xffffffff))]
    cases += [dict(kind=k,result=r,handle=h) for k in ('resume','suspend','terminate') for h,r in ((0,0),(0x12345678,0),(0x12345678,1),(0x12345678,0xffffffff))]
    methods={'create':0x6be490,'wait':0x6be4c0,'running':0x6be4f0,'resume':0x6be530,'suspend':0x6be550,'terminate':0x6be570}
    try:
        for c in cases:
            report['pending']=c;save()
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;o=f.call(0x6be5c0);assert f.allocations[o]==36 and p.uint(o+4)==0x7291e0
            p.put_uint(o+0x20,c['handle']);p.mu.mem_write(o+0x1c,b'\xff');before=bytes(p.mu.mem_read(o,36));expected=bytearray(before);events=[]
            base=0x34180000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
            for i,(iat,(name,argc)) in enumerate(imports.items()):
                address=base+16*(i+1)
                def seam(m,name=name,argc=argc):
                    args=[m.uint(m.reg('ESP')+4*(j+1)) for j in range(argc)]
                    if name=='CreateThread':
                        assert args[:5]==[0,0,0x11223344,o,4];m.put_uint(args[5],0x13579bdf)
                    elif name=='GetExitCodeThread':
                        assert args[0]==c['handle'];m.put_uint(args[1],c['exitCode'])
                    else:assert args==([c['handle'],0xffffffff] if argc==2 else [c['handle']])
                    events.append(dict(importName=name,arguments=args,result=c['result'],exitCode=c.get('exitCode')));m.fixture_return(4*argc,eax=c['result'])
                p.put_uint(iat,address);p.seams[address]=seam
            args=(0x11223344,0xaabbccdd) if c['kind']=='create' else (0xffffffff,) if c['kind'] in ('wait','terminate') else ()
            value=f.call(methods[c['kind']],o,args)&255
            if c['kind']=='create':
                wanted=int(c['result']!=0);struct.pack_into('<I',expected,0x18,0xaabbccdd);struct.pack_into('<I',expected,0x20,c['result'])
            elif c['kind']=='wait':wanted=int(not c['handle'] or c['result']!=0x102)
            elif c['kind']=='running':
                wanted=int(bool(c['handle']) and c['exitCode']==0x103)
                if c['handle'] and not wanted:struct.pack_into('<I',expected,0x20,0)
            elif c['kind']=='terminate':wanted=int(bool(c['handle']) and c['result']==1)
            else:
                wanted=int(bool(c['handle']))
                if c['kind']=='resume' and c['handle']:expected[0x1c]=0
            assert value==wanted and bytes(p.mu.mem_read(o,36))==bytes(expected)
            assert len(events)==int(c['kind']=='create' or bool(c['handle']))
            p.put_uint(o+0x20,0);f.call(p.uint(p.uint(o+4)),o+4,(1,));assert set(f.allocations)==set(f.freed)
            report['cases'].append(dict(input=c,result=value,wholeObjectBefore=before.hex(),wholeObjectAfter=bytes(expected).hex(),events=events));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',pending=report.get('pending'),error=str(error))));return 1
    print(json.dumps(dict(status='passed',cases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
