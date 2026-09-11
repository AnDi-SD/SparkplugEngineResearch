"""Original command construction/reset and AIAction callers over literal graphs."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ai-entry-reset'
CATALOG={c['className'][2:-8]:c for c in json.loads((ROOT/'research/ai-action-construction-contracts-2026-09-10.json').read_text())['classes']}
ZERO_BYTES=list(range(0x19,0x32))+list(range(0x5c,0x61))+[0x64,0x70]
ZERO_WORDS=[4,8,12,16,20,0x6c]
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';kind=c['kind'];name=c.get('name');calls=[];query_calls=[]
    entry=(0x599430 if pc else 0x3731d0) if kind=='constructor' else int(CATALOG[name][k]['slots'][10 if name=='Idle' else 11],16)
    if pc:
        p=PcBlocks();p.fixture_seh_chain();base=p.allocate(0x4000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        ranges=[(0x372f90,0x150),(0x3730e0,0x9c),(0x3731d0,0x78)] if kind=='constructor' else [(entry,0x90),(0x3730e0,0x9c)]
        if name=='Idle':ranges += [(0x25ae00,0x70),(0x36ebc0,0x180)]
        p=Ps2ScalarPrefix(ranges,stack_window=(0x22000000,4096),upper64=UPPER);base=0x21000000;p.map(base,0x4000);p.map(0x22000000,4096)
        read,write,u=p.read,p.write,p.u;p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        for i in range(16,21):p.reg(str(i),0x56780000+i)
        raw,sections=pristine()
        tables=[(0x494710,16)]
        if name=='Idle':tables.append((int(CATALOG[name]['ps2']['vtable'],16),0x40))
        if kind=='constructor':tables.append((0x476f50,12))
        for a,n in tables:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
    obj,owner,char,command,profile,perception=[base+a for a in (0,0x1000,0x1400,0x1800,0x2000,0x3000)]
    put=p.put_uint;write(base,bytes([c['fill']])*0x4000)
    if kind=='constructor':
        if pc:assert read(0x7600e0,12)==bytes(12)
    else:
        put(obj,int(CATALOG[name][k]['vtable'],16));put(obj+(0x20 if pc else 0x24),owner);put(owner+(0x144 if pc else 0x154),char)
        put(char+(0x130 if pc else 0x13c),command if c.get('commandPresent',True) else 0);put(command,0x703090 if pc else 0x494710)
        assert p.uint(p.uint(command)+(4 if pc else 12))==(0x5992e0 if pc else 0x3730e0)
        if name=='IcyAttack':
            glob=0x765ad4 if pc else 0x49fc7c
            if not pc:p.map(glob,4)
            put(glob,profile)
        if name=='Idle':
            write(owner+(0x14d if pc else 0x15d),bytes([c['permission']]))
            put(char+(0x154 if pc else 0x160),perception);put(perception+0x10,0)
    before=read(base,0x4000);expected=bytearray(before)
    def ew(a,v):struct.pack_into('<I',expected,a-base,v&0xffffffff)
    def eb(a,v):expected[a-base]=v
    resets=kind=='constructor' or c.get('commandPresent',True)
    if resets:
        for off in ZERO_WORDS:ew(command+off,0)
        for off in ZERO_BYTES:eb(command+off,0)
    if kind=='constructor':
        ew(command,0x703090 if pc else 0x494710);eb(command+0x18,0 if pc else 1)
        for off in range(0x44,0x4b):eb(command+off,c['fill'])
        eb(command+0x4b,int(c['fill']!=0))
        for off in list(range(0x4c,0x53))+[0x61,0x62,0x63]:eb(command+off,0)
        ew(command+0x54,0x7f7fffff)
        for off in [0x34,0x38,0x3c,0x40,0x58,0x68]:ew(command+off,0)
    elif name!='Idle':
        ew(obj+(0x3a8 if pc else 0x3ac),0)
        if name=='IcyAttack':eb(profile+0x510,1)
    def observe(machine,address,size,user):
        if address==(0x5992e0 if pc else 0x3730e0):calls.append(p.reg('ECX' if pc else 'A0'))
        elif address==(0x4f1910 if pc else 0x36ebc0):query_calls.append(p.reg('ECX' if pc else 'A0'))
    hooks=[u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_CODE,observe,begin=a,end=a) for a in ((0x5992e0,0x4f1910) if pc else (0x3730e0,0x36ebc0))]
    if pc:
        p.run(entry,this=command if kind=='constructor' else obj,args=(c.get('argument',0),) if name=='Idle' else ())
        result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
        if kind=='constructor':assert p.reg('EAX')==command
    else:
        p.reg('A0',command if kind=='constructor' else obj);p.reg('A1',c.get('argument',0));original=[read(a,n) for a,n in p.ranges]
        result=p.run(entry,[p.RETURN],count=6000,timeout_us=500000);assert original==[read(a,n) for a,n in p.ranges]
        assert p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN
        assert [p.reg(str(i)) for i in range(16,21)]==[0x56780000+i for i in range(16,21)]
        assert result['initialUpper64']==result['finalUpper64']
        if kind=='constructor':assert p.reg('V0')==command
    for hook in hooks:u.hook_del(hook)
    assert calls==([command] if resets else []),calls
    assert query_calls==([perception] if name=='Idle' and c['permission'] else []),query_calls
    after=read(base,0x4000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(commandResetCalls=len(calls),perceptionQueryCalls=len(query_calls),commandTable=f'{p.uint(command):08X}',commandBytes=read(command,0x74).hex(),guardedBytes=0x4000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)))
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'reset-cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter()
    report=dict(kind='original-command-reset-and-ai-callers',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
    for c in cases:
        for k in c.get('platforms',('pc','ps2')):
            row=dict(input=c,platform=k);report['cases'].append(row)
            try:row.update(status='passed',**execute(c,k))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            if row['status']!='passed':break
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==sum(len(c.get('platforms',('pc','ps2'))) for c in cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
