#!/usr/bin/env python3
"""Original AIAction command resets and adjacent input methods, PC/PS2.

The pointer graph is explicit borrowed input storage, not constructed gameplay.
No game callback is replaced. Full buffers are checked, including untouched
bytes. A failed case is preserved and does not suppress independent cases.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime

CATALOG=ROOT/'research/ai-action-construction-contracts-2026-09-10.json'
# Reviewed destination command bytes and optional own zero word (PC offset).
# Repeated PC addresses are retained as separate class/layout qualifications.
RESETS={
 'Attack':([0x1d,0x20,0x21,0x1f],0x3a8),
 'BacoAttack':([0x1d,0x20],0x3a8),
 'DarcyAttack':([0x1d,0x20,0x21,0x22,0x23,0x24],0x3a8),
 'DroidAttack':([0x1d,0x20,0x21],0x3a8),
 'DroidWander':([0x1c],None),
 'FrogAttack':([0x1d,0x20],0x3a8),
 'Hurt':([0x5c],None),
 'IceGargoyleAttack':([0x20,0x21],0x3d8),
 'IceGargoyleClaw':([0x20,0x21],0x3ac),
 'IceGargoyleSleeping':([0x1b],None),
 'IceGargoyleWithdrawl':([0x20,0x21],0x3ac),
 'IceWormAttack':([0x1d,0x20],0x3a8),
 'KnutAttack':([0x1d,0x5c,0x20,0x21,0x22,0x23,0x24],0x3a8),
 'MinotaurAttack':([0x1d,0x20],0x3a8),
 'MinotaurDefend':([0x1d,0x1f],0x3a8),
 'MosquitoAttack':([0x1d,0x20],0x3a8),
 'ShadowBeastAttack':([0x1d,0x1f,0x20,0x21,0x22],0x3a8),
 'SpiderAttack':([0x1d,0x20,0x21],0x3a8),
 'TrollAttack':([0x1d,0x20,0x21,0x22,0x23,0x24],0x3a8),
 'TrollBeforeFight':([0x1d,0x20,0x21,0x22,0x23,0x24],None),
}
DIRECT={'FrogAttack':0x3e8,'MinotaurAttack':0x3c0,'MinotaurDefend':0x3b4}
INPUT_WORDS={'FlyingWander':[0x3b8,0x3bc],'IceGargoyleSleeping':[0x3a8,0x3ac],
             'TrollBeforeFight':[0x3a8,0x3ac,0x3b0,0x3b4]}


def execute(row,platform,kind,fill,argument=0,command_present=True):
    name=row['className'][2:-8];ispc=platform=='pc';delta=0 if ispc else 4
    size=row[platform]['allocationBytes'];slot=11 if kind=='reset' else 10
    entry=int(row[platform]['slots'][slot],16)
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;storage=p.allocate(0x3000)
        write=lambda a,b:p.mu.mem_write(a,bytes(b))
        read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(entry,0x100)]);storage=0x21000000;p.map(storage,0x3000)
        write,read,put=p.write,p.read,p.put_uint
    offsets={'action':0,'owner':0x1000,'character':0x1400,'command':0x1800,'perception':0x1c00,'move':0x2000}
    extents={'action':size,'owner':0x300,'character':0x300,'command':0x100,'perception':0x100,'move':0x300}
    addresses={k:storage+v for k,v in offsets.items()}
    for key,a in addresses.items():write(a,bytes([fill])*extents[key])
    obj,owner,char,cmd,perception,move=[addresses[k] for k in offsets]
    put(obj,int(row[platform]['vtable'],16));put(obj+0x20+delta,owner)
    put(owner+(0x144 if ispc else 0x154),char)
    put(char+(0x130 if ispc else 0x13c),cmd if command_present else 0)
    put(char+(0x154 if ispc else 0x160),perception)
    put(char+(0x12c if ispc else 0x138),move)
    if name in DIRECT:put(obj+DIRECT[name]+delta,cmd)
    before={k:read(a,extents[k]) for k,a in addresses.items()}
    expected={k:bytearray(b) for k,b in before.items()}
    if kind=='reset':
        bs,word=RESETS[name]
        for off in bs:expected['command'][off]=0
        if word is not None:struct.pack_into('<I',expected['action'],word+delta,0)
        if name in ('DroidAttack','SpiderAttack','ShadowBeastAttack'):
            expected['perception'][0x70]=1;struct.pack_into('<I',expected['perception'],0x74,0x41c80000)
            expected['perception'][0x78]=0
        if name=='MosquitoAttack':struct.pack_into('<I',expected['move'],0x1e0+(0 if ispc else 12),0)
    elif kind=='input-clear':
        for off in INPUT_WORDS[name]:struct.pack_into('<I',expected['action'],off+delta,0)
    elif kind=='hurt-input' and command_present:
        expected['command'][0x5c]=1;expected['command'][0x60]=int(argument!=0)
        struct.pack_into('<I',expected['command'],4,0);expected['action'][0x3a8+delta]=0
    args=() if kind=='reset' else (argument,)
    if ispc:
        f.call(entry,obj,args);execution=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',obj);p.reg('A1',argument);execution=p.run(entry,[p.RETURN])
    after={k:read(a,extents[k]) for k,a in addresses.items()}
    for key in addresses:
        assert after[key]==expected[key],(key,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after[key],expected[key])) if a!=b][:12])
    execution.update(buffers={k:dict(bytes=extents[k],beforeSha256=hashlib.sha256(before[k]).hexdigest().upper(),
        afterSha256=hashlib.sha256(after[k]).hexdigest().upper(),changes=[dict(offset=i,before=a,after=b) for i,(a,b) in enumerate(zip(before[k],after[k])) if a!=b]) for k in addresses})
    return execution


def guest(output,selection='batch'):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    if selection not in ('pilot','batch'):raise ValueError('Explicit pilot or batch')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    catalog=json.loads(CATALOG.read_text());rows={c['className']:c for c in catalog['classes']}
    cases=[dict(name=n,kind='reset',fill=0xa5,argument=0,command_present=True) for n in RESETS]
    cases += [dict(name=n,kind='input-clear',fill=0x5a,argument=v,command_present=True) for n in INPUT_WORDS for v in (0,0xffffffff)]
    cases += [dict(name='Hurt',kind='hurt-input',fill=0xa5,argument=v,command_present=present) for v in (0,1,0x100,0x80000000,0xffffffff) for present in (False,True)]
    cases=cases[:1] if selection=='pilot' else cases[1:]
    report=dict(kind='paired-original-ai-action-command-protocol',status='running',selection=selection,inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),catalogSha256=hashlib.sha256(CATALOG.read_bytes()).hexdigest().upper(),
        scope='Six separate guarded borrowed input records;original methods execute without replaced game callbacks. No constructor,whole-game transition or null-chain safety claim.',cases=[])
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for case in cases:
        result=dict(input=case,platforms={});report['cases'].append(result)
        for platform in ('pc','ps2'):
            report['pending']=dict(**case,platform=platform);save()
            try:result['platforms'][platform]=dict(status='passed',**execute(rows['wx'+case['name']+'AIAction'],platform,**{k:v for k,v in case.items() if k!='name'}))
            except Exception as error:result['platforms'][platform]=dict(status='blocked',error=str(error),traceback=traceback.format_exc())
            report.pop('pending');save()
    failed=sum(r['status']!='passed' for c in report['cases'] for r in c['platforms'].values())
    report['status']='passed' if not failed else 'partial';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),platformCases=2*len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
