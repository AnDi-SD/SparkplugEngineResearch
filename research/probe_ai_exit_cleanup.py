"""Twenty original AIAction exit contracts on independently guarded linked records."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ai-exit-cleanup'
CATALOG={x['className'][2:-8]:x for x in json.loads((FOLDER/'selection.json').read_text())}
FLAGS={
 'Attack':[0x1d,0x20,0x21,0x1f], 'BacoAttack':[0x1d,0x20],
 'DarcyAttack':[0x1d,0x20,0x21,0x22,0x23,0x24], 'DroidAttack':[0x1d,0x20,0x21],
 'DroidWander':[0x1c], 'FrogAttack':[0x1d,0x20], 'Hurt':[0x5c],
 'IceGargoyleAttack':[0x20,0x21], 'IceGargoyleClaw':[0x20,0x21],
 'IceGargoyleSleeping':[0x1b], 'IceGargoyleWithdrawl':[0x20,0x21],
 'IceWormAttack':[0x1d,0x20], 'KnutAttack':[0x1d,0x5c,0x20,0x21,0x22,0x23,0x24],
 'MinotaurAttack':[0x1d,0x20], 'MinotaurDefend':[0x1d,0x1f],
 'MosquitoAttack':[0x1d,0x20], 'ShadowBeastAttack':[0x1d,0x1f,0x20,0x21,0x22],
 'SpiderAttack':[0x1d,0x20,0x21], 'TrollAttack':[0x1d,0x20,0x21,0x22,0x23,0x24],
 'TrollBeforeFight':[0x1d,0x20,0x21,0x22,0x23,0x24]}
CACHED={'FrogAttack':0x3e8,'MinotaurAttack':0x3c0,'MinotaurDefend':0x3b4}
SENSOR={'DroidAttack','SpiderAttack','ShadowBeastAttack'}
KEEP_TARGET={'DroidWander','Hurt','IceGargoyleSleeping','TrollBeforeFight'}
def sha(b):return hashlib.sha256(b).hexdigest().upper()

def execute(c,k):
    pc=k=='pc';name=c['name'];entry=int(CATALOG[name][k]['slots'][11],16);calls=[]
    if pc:
        p=PcBlocks();teb=p.fixture_seh_chain();base=p.allocate(0x3000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu;uc=p.uc
    else:
        p=Ps2ScalarPrefix([(entry,0x180)]);base=0x21000000;p.map(base,0x3000);p.map(0x22000000,4096);p.reg('SP',0x22000800);read,write=p.read,p.write;u=p.u
        import unicorn as uc
    obj,owner,char,command,cached,perception,move=[base+a for a in (0,0x800,0xc00,0x1000,0x1200,0x1400,0x1800)]
    put=p.put_uint;write(base,bytes([c['fill']])*0x3000);delta=0 if pc else 4
    put(obj,int(CATALOG[name][k]['vtable'],16));put(obj+0x20+delta,owner);put(owner+(0x144 if pc else 0x154),char)
    put(char+(0x130 if pc else 0x13c),command);put(char+(0x154 if pc else 0x160),perception);put(char+(0x12c if pc else 0x138),move)
    if name in CACHED:put(obj+CACHED[name]+delta,cached)
    before=read(base,0x3000);expected=bytearray(before);cmd=cached if name in CACHED else command
    for off in FLAGS[name]:expected[cmd-base+off]=0
    if name not in KEEP_TARGET:
        off=0x3d8 if name=='IceGargoyleAttack' else 0x3ac if name in ('IceGargoyleClaw','IceGargoyleWithdrawl') else 0x3a8
        struct.pack_into('<I',expected,off+delta,0)
    if name in SENSOR:
        expected[perception-base+0x70]=1;struct.pack_into('<f',expected,perception-base+0x74,25.);expected[perception-base+0x78]=0
    if name=='MosquitoAttack':struct.pack_into('<I',expected,move-base+(0x1e0 if pc else 0x1ec),0)
    if pc:
        def observe(u,address,size,user):
            assert p.reg('ECX')==char and p.uint(p.reg('ESP')+4)==0;calls.append('original-perception-getter')
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=0x515350,end=0x515350)
        p.run(entry,this=obj);assert p.uint(teb)==0xffffffff
        result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()),executionLimits=p.last_execution_limits)
    else:
        p.reg('A0',obj);original=[read(a,n) for a,n in p.ranges];saved={r:p.reg(r) for r in ('S0','S1','SP','RA')};result=p.run(entry,[p.RETURN],count=2000,timeout_us=500000)
        assert original==[read(a,n) for a,n in p.ranges] and saved=={r:p.reg(r) for r in saved}
    assert calls==(['original-perception-getter']*2 if pc and name in SENSOR else [])
    after=read(base,0x3000);assert after==bytes(expected),[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:15]
    result.update(originalCallbacks=calls,commandSource='distinct cached record' if name in CACHED else 'owner character record',clearedCommandBytes=FLAGS[name],targetPreserved=name in KEEP_TARGET,guardedBytes=0x3000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),scope='Direct original exits and actual PC cached-perception getter;no game callback returns,allocator or full AI gameplay. Cached command is deliberately different from current character command.Whole guarded graph and untouched fields compared. PS2 ordinary scalar instructions;no stack extension.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-ai-exit-cleanup',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),catalogSha256=sha((FOLDER/'selection.json').read_bytes()),cases=[])
    for c in cases:
        for k in c['platforms']:
            row=dict(input=c,platform=k);report['cases'].append(row)
            try:row.update(status='passed',**execute(c,k))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            if row['status']!='passed':break
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==sum(len(c['platforms']) for c in cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')

if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
