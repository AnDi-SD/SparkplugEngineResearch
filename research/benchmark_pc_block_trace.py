#!/usr/bin/env python3
"""Fresh original-operation benchmark with exact state digests across tracers."""
from pathlib import Path
import hashlib,json,sys,time,traceback
from unittest.mock import patch
from pc_instruction_emulator import ROOT,BASE,STACK,ARENA_SIZE,PcInstructions,run_bounded
from pc_block_emulator import PcBlocks
import probe_pc_animation_lifecycle as lifetime
from probe_pc_task_timer import TimerFixture


def guest(mode,case,output):
    if mode not in ('instruction','block') or case not in ('machine-lifecycle','bloom-1m','bloom-6m','ai-action-cold'):raise ValueError('Explicit bounded benchmark selection required')
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local report required')
    started=time.perf_counter()
    with patch.object(lifetime,'PcInstructions',PcBlocks if mode=='block' else PcInstructions):f=TimerFixture()
    p=f.p;f.time(1200)
    report=dict(kind='original-pc-tracer-benchmark',mode=mode,case=case,status='running',stages=[],
        evidenceUnits='basic-block-entry' if mode=='block' else 'instruction-entry',
        guardScope='Whole-block prevalidation may reject a denied block before earlier ordinary instructions in that block; no full instruction trace claim in block mode.',
        sources={name:hashlib.sha256((ROOT/'research'/name).read_bytes()).hexdigest().upper() for name in ('benchmark_pc_block_trace.py','pc_instruction_emulator.py','pc_block_emulator.py','probe_pc_animation_lifecycle.py','probe_pc_task_timer.py')})
    def save():
        report['seconds']=time.perf_counter()-started;output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def digest(data):return hashlib.sha256(data).hexdigest().upper()
    def semantic():
        return dict(registers={n:p.reg(n) for n in ('EIP','ESP','EAX','EBX','ECX','EDX','ESI','EDI','EBP','EFLAGS','FPCW','FPSW','FPTAG')},
            x87=[p.mu.reg_read(getattr(p.xr,'UC_X86_REG_FP'+str(i))) for i in range(8)],
            imageSha256=digest(bytes(p.mu.mem_read(BASE,p.size))),stackSha256=digest(bytes(p.mu.mem_read(STACK,ARENA_SIZE))),
            arenaSha256=digest(bytes(p.mu.mem_read(0x31000000,p.arena_size))),arenaUsed=p.allocated,
            allocationRequests=f.requests,freed=f.freed,clonePairs=f.clone_pairs,timerRefreshes=f.refreshes)
    def call(label,address,this=0,args=()):
        report['pending']=dict(label=label,address=f'{address:08X}');save();at=time.perf_counter()
        try:value=f.call(address,this,args);status='returned';error=None
        except Exception as e:value=None;status='stopped';error=str(e)
        item=dict(label=label,address=f'{address:08X}',status=status,seconds=time.perf_counter()-at,value=value,
                  observedEntries=sum(p.visits.values()),limits=p.last_execution_limits,state=semantic())
        if error:item['error']=error
        report['stages'].append(item);report.pop('pending');save();return value,status,error
    try:
        if case=='machine-lifecycle':
            obj,status,error=call('factory',0x4016a0);assert status=='returned',error
            record,status,error=call('rtti',p.uint(p.uint(obj)+16),obj);assert status=='returned' and record==0x74e728,error
            clone,status,error=call('clone',p.uint(p.uint(obj)+8),obj);assert status=='returned',error
            for label,owner in [('delete-clone',clone),('delete-original',obj)]:
                value,status,error=call(label,p.uint(p.uint(owner)),owner,(1,));assert status=='returned' and owner in f.freed,error
        else:
            p.execution_profile='file' if case=='bloom-1m' else 'protected-constructor'
            value,status,error=call('factory',0x590050 if case=='ai-action-cold' else 0x4044c0)
            assert status=='stopped',status
            assert ('invalid memory' if case=='ai-action-cold' else 'instruction/time cap') in error,error
        report['semanticSha256']=digest(json.dumps([dict(label=s['label'],status=s['status'],value=s['value'],state=s['state']) for s in report['stages']],sort_keys=True).encode())
        report.update(status='captured',bodySeconds=sum(s['seconds'] for s in report['stages']),validationCacheEntries=len(getattr(p,'block_cache',p.code_cache)));save()
    except Exception as error:
        report.update(status='failed',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='failed',error=str(error))));return 1
    print(json.dumps({k:report[k] for k in ('status','mode','case','bodySeconds','seconds','semanticSha256')}));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
