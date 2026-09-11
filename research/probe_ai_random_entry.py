"""Actual attack RNG calls: PC whole methods and bounded PS2 pre-ISA limits."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
import probe_pc_animation_lifecycle as lifetime

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ai-random-entry'
STATE=ROOT/'local-data/results/native-cycle-20260911-0730/rng-transactions/cold-pilot-run1.json'
CATALOG={c['className'][2:-8]:c for c in json.loads((ROOT/'research/ai-action-construction-contracts-2026-09-10.json').read_text())['classes']}
LIMITS={'MinotaurAttack':0x25f358,'MinotaurDefend':0x25fd28,'MosquitoAttack':0x26061c}
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';name=c['name'];entry=int(CATALOG[name][k]['slots'][10],16);outputs=[];calls=[]
    if pc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;base=p.allocate(0x6000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        p=Ps2ScalarPrefix([(entry,0x200),(0x36ebc0,0x180),(0x14e640,0x28),(0x108350,0x1c),(0x108e30,0x68)],profile='integer-squares',stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,0x6000);p.map(0x22000000,4096);read,write,u=p.read,p.write,p.u;p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        raw,sections=pristine();p.map(0x496780,0x6c);write(0x496780,read_window('ps2',raw,0x496780,0x6c,sections)[0])
    obj,owner,char,cmd,perception,entity,node,sentinel,link,target,te,tn,timer,move,cam,view,cn=[base+x for x in (0,0x1000,0x1400,0x1800,0x1c00,0x2000,0x2400,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x3000,0x3800,0x4400,0x4800,0x4c00)]
    put=p.put_uint;write(base,bytes([c['fill']])*0x6000);delta=0 if pc else 4
    put(obj,int(CATALOG[name][k]['vtable'],16));put(obj+0x20+delta,owner);put(owner,0x702708 if pc else 0x496780)
    put(owner+(0x144 if pc else 0x154),char);put(char+(0x130 if pc else 0x13c),cmd);put(char+(0x154 if pc else 0x160),perception);put(char+(0x12c if pc else 0x138),move)
    put(owner+0x24,entity);put(entity+0x24,node);put(target+0x24,te);put(te+0x24,tn)
    write(node+0x20,struct.pack('<3f',0,0,0));write(tn+0x20,struct.pack('<3f',3,4,0));put(perception+0x10,owner)
    if pc:put(perception+0x18,sentinel)
    else:sentinel=perception+0x18
    forward,backward=(0,4) if pc else (4,0)
    put(sentinel+forward,link if c['target'] else sentinel);put(sentinel+backward,link if c['target'] else sentinel)
    put(link+forward,sentinel);put(link+backward,sentinel);put(link+8,target)
    globals=[(0x755298 if pc else 0x49fc80,timer),(0x765af0 if pc else 0x49fc78,cam)]
    for a,v in globals:
        if not pc:p.map(a,4)
        put(a,v)
    put(timer+0x1c,c['now']);put(cam+0x28,0 if c['nullView'] else view);put(view+0x18,cn)
    write(obj+0x3b8+delta,struct.pack('<f',c['amplitude']))
    state=json.loads(STATE.read_text())['cases'][0]['platforms'][k];array_bytes=bytes.fromhex(state['arrayHex'])
    index,array=(0x73fe8c,0x755658) if pc else (0x49c230,0x4a0e80)
    assert 0<=c['index']<=622 and len(array_bytes)==624*(4 if pc else 8)
    if not pc:p.map(index,4);p.map(array-16,len(array_bytes)+32)
    write(array-16,b'\xa5'*(len(array_bytes)+32));write(array,array_bytes);put(index,c['index'])
    before=read(base,0x6000);expected=bytearray(before)
    def ew(a,v):struct.pack_into('<I',expected,a-base,v&0xffffffff)
    def eb(a,v):expected[a-base]=v
    def ef(a,v):struct.pack_into('<f',expected,a-base,v)
    if c['target']:expected[0x1c30:0x1c3c]=struct.pack('<3f',3,4,0)
    ew(obj+0x3a8+delta,target if c['target'] else 0);ew(obj+0x3ac+delta,4 if name=='MosquitoAttack' else 0)
    if name!='MinotaurDefend':eb(cmd+0x1d,1)
    if name=='MinotaurAttack':ew(obj+0x3b0+delta,0);eb(obj+0x3bc+delta,0);ew(obj+0x3c0+delta,cmd)
    if name=='MosquitoAttack':ew(obj+0x3b0+delta,c['now']+1000)
    selector=not c['target'] and (pc or name=='MinotaurAttack')
    stop=(0x591c80 if pc else 0x225df0) if selector else None if pc else LIMITS[name]
    returns={0x5b8fc6,0x5b8a86,0x5b4bbc,0x5b4bf8} if pc else {0x25f300,0x25fd24,0x260618}
    def observe(machine,address,size,user):
        if address in returns:outputs.append(p.reg('EAX' if pc else 'V0')&0xffffffff)
        elif address==(0x4f1910 if pc else 0x36ebc0):calls.append('perception')
        elif address==(0x4132b0 if pc else 0x108350):calls.append('rng')
    hooks=[u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_CODE,observe,begin=a,end=a) for a in returns|({0x4f1910,0x4132b0} if pc else {0x36ebc0,0x108350})]
    if pc:
        p.run(entry,this=obj,args=(c['argument'],),stop_at=stop);result=dict(entry=f'{entry:08X}',stop=None if stop is None else f'{stop:08X}',completion='actual selector boundary' if selector else 'original return',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',obj);p.reg('A1',c['argument']);original=[read(a,n) for a,n in p.ranges];result=p.run(entry,[stop],count=6000,timeout_us=500000);assert original==[read(a,n) for a,n in p.ranges]
        result.update(unsupportedWord=f'{p.uint(stop):08X}' if not selector else None,boundaryRegisters={n:f'{p.reg(n):016X}' for n in ('V0','S0','S1','F0','F1','F2','F3')})
    for hook in hooks:u.hook_del(hook)
    count=0 if name=='MinotaurAttack' and not c['target'] else 2 if pc and name=='MosquitoAttack' else 1
    assert len(outputs)==count and calls==['perception']+['rng']*count,(outputs,calls)
    assert p.uint(index)==c['index']+count and read(array,len(array_bytes))==array_bytes
    assert read(array-16,16)==read(array+len(array_bytes),16)==b'\xa5'*16
    if pc:
        if name=='MinotaurAttack' and outputs:ef(obj+0x3b4,250000.+outputs[0]*(240000./4294967296.))
        elif name=='MinotaurDefend':ew(obj+0x3b0,c['now']+3000+outputs[0]%1501);ew(obj+0x3b4,cmd)
        elif name=='MosquitoAttack':
            ew(obj+0x3b4,c['now']+500+outputs[0]%501);ef(obj+0x3b8,-c['amplitude']);ef(move+0x254,100.+outputs[1]*(150./4294967296.));ew(obj+0x3bc,0x47afc800);eb(obj+0x3c4,0)
            if c['target']:ew(move+0x1e0,0 if c['nullView'] else cn)
    consumer=None
    if selector:
        consumer=dict(receiver=p.reg('ECX' if pc else 'A0'),key=p.uint(p.reg('ESP')+4) if pc else p.reg('A1')&0xffffffff,parameter=p.uint(p.reg('ESP')+8) if pc else p.reg('A2')&0xffffffff)
        assert consumer==dict(receiver=owner,key=1,parameter=0)
    after=read(base,0x6000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(boundary='selector' if selector else None if pc else 'unsupported-arithmetic',consumer=consumer,rngOutputs=outputs,rngInputIndex=c['index'],rngOutputIndex=p.uint(index),nativeStateSourceSha256=sha(STATE.read_bytes()),arraySha256=sha(array_bytes),guardedBytes=0x6000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)))
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter()
    report=dict(kind='original-ai-random-entry',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
    for c in cases:
        for k in ('pc','ps2'):
            row=dict(input=c,platform=k);report['cases'].append(row)
            try:row.update(status='passed',**execute(c,k))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            if row['status']!='passed':break
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==2*len(cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
