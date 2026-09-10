#!/usr/bin/env python3
"""Original AIAction integer dispatch, stopped at each actual state body.

Tables come from pristine bytes at construction-proven vtables. No state
callback is substituted or reported as executed beyond its entry boundary.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,PC,read_window,pefile
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine

CATALOG=ROOT/'research/ai-action-construction-contracts-2026-09-10.json'
SPECS={
 'IceGargoyleSleeping':dict(pc=0x5aef90,ps2=0x2528f0,pcWord=0x3a8,ps2Bytes=0x58,
     states=[0,1,2,0xffffffff,0x80000000],slots={0:18,1:19},default=None),
 'TrollBeforeFight':dict(pc=0x5b74d0,ps2=0x26b6c0,pcWord=0x3a8,ps2Bytes=0xac,
     states=[0,1,2,3,4,0xffffffff,0x80000000],slots={0:17,1:18,2:19,3:20},default=17),
 'TrollAttack':dict(pc=0x5b62e0,ps2=0x26a950,pcWord=0x3ac,ps2Bytes=0x148,
     states=list(range(11))+[11,0xffffffff,0x80000000],slots={0:18,1:19,2:20,3:21,4:22,5:23,6:28,7:24,8:25,9:26,10:27},default=18),
}


def guest(output,selection='all'):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    if selection not in ('all','corrected-troll-8-9'):raise ValueError('Explicit selection required')
    raw=PC.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['pc'];pe=pefile.PE(data=raw,fast_load=True)
    psraw,sections=pristine();rows={c['className']:c for c in json.loads(CATALOG.read_text())['classes']}
    report=dict(kind='paired-original-ai-action-state-dispatch',status='running',inputs=EXPECTED,cases=[],selection=selection,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Actual native table and branch selection. Stops before state body;no replacement callee. Sleeping unrecognized states return normally. PS2 Troll starts after saved-register prologue;no full Tick/timers/attacks claim.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for name,spec in SPECS.items():
        if selection=='corrected-troll-8-9' and name!='TrollAttack':continue
        row=rows['wx'+name+'AIAction']
        for state in spec['states']:
            if selection=='corrected-troll-8-9' and state not in (8,9):continue
            result=dict(className=row['className'],state=state,expectedSlot=spec['slots'].get(state,spec['default']),platforms={})
            report['cases'].append(result)
            for platform in ('pc','ps2'):
                report['pending']=dict(className=row['className'],state=state,platform=platform);save()
                try:
                    ispc=platform=='pc';vt=int(row[platform]['vtable'],16);size=row[platform]['allocationBytes'];slot=result['expectedSlot']
                    vt_raw=read_window(platform,raw if ispc else psraw,vt,124,pe if ispc else sections)[0]
                    target=struct.unpack_from('<I',vt_raw,4*(slot+(0 if ispc else 2)))[0] if slot is not None else None
                    if ispc:
                        p=PcBlocks();obj=p.allocate(size);write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
                    else:
                        ranges=[(spec['ps2'],spec['ps2Bytes'])]
                        if name=='TrollAttack':ranges.append((0x478500,44))
                        p=Ps2ScalarPrefix(ranges);obj=0x21000000;p.map(obj,size);p.map(0x22000000,4096);p.map(vt,124)
                        write,read,put=p.write,p.read,p.put_uint;write(vt,vt_raw)
                        p.reg('SP',0x22000800);p.reg('A0',obj)
                    write(obj,b'\xa5'*size);put(obj,vt);put(obj+spec['pcWord']+(0 if ispc else 4),state);before=read(obj,size)
                    if ispc:
                        p.run(spec['pc'],this=obj,stop_at=target)
                        execution=dict(entry=f"{spec['pc']:08X}",completion='actual original state-body entry' if target else 'original return',blocks=sum(p.visits.values()))
                        assert (p.reg('ECX')==obj if target else p.reg('EAX')&255==1)
                    else:
                        execution=p.run(spec['ps2'],[target if target else p.RETURN])
                        assert (p.reg('A0')==obj if target else p.reg('V0')==1)
                    assert read(obj,size)==before,'whole action remains unchanged before state body'
                    execution.update(status='passed',target=f'{target:08X}' if target else None,slot=slot,receiverPreserved=True,actionSha256=hashlib.sha256(before).hexdigest().upper())
                    result['platforms'][platform]=execution
                except Exception as error:result['platforms'][platform]=dict(status='blocked',error=str(error),traceback=traceback.format_exc())
                report.pop('pending');save()
    failed=sum(r['status']!='passed' for c in report['cases'] for r in c['platforms'].values())
    report['status']='passed' if not failed else 'partial';save()
    print(json.dumps(dict(status=report['status'],pairedCases=len(report['cases']),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
