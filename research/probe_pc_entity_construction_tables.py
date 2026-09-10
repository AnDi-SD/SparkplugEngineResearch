#!/usr/bin/env python3
"""Observe original constructor vtable writes; RTTI ancestry is separate."""
import hashlib,json,sys,time
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local evidence required')
    folder=ROOT/'local-data/results/native-cycle-20260910-1900'
    catalog=json.loads((folder/'catalog/pc-architecture.json').read_text())['registered_types']
    byrecord={x['registration_object_va']:x for x in catalog}
    byname={x['class_name']:x for x in catalog}
    report=dict(kind='original-pc-constructor-vtable-writes',sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Six fresh factory-only guests;original vtable writes and literal RTTI getter identity. No full C++ hierarchy or destructor assertion.',cases=[])
    started=time.perf_counter()
    for name in ('wxEntity','wxProjectile','wxKickableIce','wxRockProjectile','wxSavePoint','wxStellaRingTrigger'):
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
        p=f.p;f.time(1200);writes=[]
        def observe(mu,access,address,size,value,userdata):
            if size!=4 or address not in f.allocations or not 0x6d0000<=value<0x740000:return
            getter=p.uint(value+0x10)
            try:code=bytes(mu.mem_read(getter,6))
            except Exception:return
            if code[:1]!=b'\xb8' or code[5:]!=b'\xc3':return
            record=int.from_bytes(code[1:5],'little')
            if record not in byrecord:return
            writes.append(dict(object=address,storeIp=f'{p.reg("EIP"):08X}',vtable=f'{value:08X}',
                getter=f'{getter:08X}',getterBytes=code.hex(),registration=f'{record:08X}',className=byrecord[record]['class_name']))
        hook=p.mu.hook_add(p.uc.UC_HOOK_MEM_WRITE,observe)
        obj=f.call(byname[name]['constructor_arg5_va']);p.mu.hook_del(hook)
        sequence=[x for x in writes if x['object']==obj]
        assert sequence and sequence[-1]['className']==name
        report['cases'].append(dict(className=name,registeredBase=byname[name]['base_class_name'],object=obj,
            allocationBytes=f.allocations[obj],writes=sequence,factoryBlocks=sum(p.visits.values())))
    report.update(status='passed',seconds=time.perf_counter()-started)
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status='passed',cases=len(report['cases']),seconds=report['seconds'])))
    return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
