"""Original nearest-perception query and AIAction v12 transition requests.

Literal guarded input graph, real perception traversal, pristine owner vtable.
State selection and Droid visibility consumers are boundaries, not fake returns.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
import probe_pc_animation_lifecycle as lifetime

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ai-control-events'
CATALOG={c['className'][2:-8]:c for c in json.loads((ROOT/'research/ai-action-construction-contracts-2026-09-10.json').read_text())['classes']}
ONE={'BacoAttack','DroidAttack','FrogAttack','MinotaurAttack','MinotaurDefend','MosquitoAttack','ShadowBeastAttack','SpiderAttack'}
ZERO={'IceGargoyleAttack','IceGargoyleClaw','IceGargoyleWithdrawl','IceWormAttack'}
TWO={'FlyingWander','Idle','Wander'}
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';name=c.get('name');query=c['kind']=='perception';entry=(0x4f1910 if pc else 0x36ebc0) if query else int(CATALOG[name][k]['slots'][12],16)
    calls=[]
    if pc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;base=p.allocate(0x6000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        ranges=[(0x36ebc0,0x180),(0x14e640,0x28)]
        if not query:ranges.append((entry,0x180))
        p=Ps2ScalarPrefix(ranges,profile='integer-squares',stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,0x6000);p.map(0x22000000,4096);read,write,u=p.read,p.write,p.u
        p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        for i in range(16,21):p.reg(str(i),0x12340000+i)
        for i in range(20,24):p.reg('F'+str(i),0x3f800000+i*0x10000)
        if not query:
            raw,sections=pristine();p.map(0x496780,0x6c);write(0x496780,read_window('ps2',raw,0x496780,0x6c,sections)[0])
    obj,owner,char,perception,entity,node,sentinel=[base+x for x in (0,0x1000,0x1400,0x1800,0x1c00,0x2000,0x2400)]
    put=p.put_uint;write(base,bytes([c.get('fill',0xa5)])*0x6000)
    put(owner,0x702708 if pc else 0x496780);put(owner+(0x144 if pc else 0x154),char)
    put(char+(0x154 if pc else 0x160),perception);put(owner+0x24,entity);put(entity+0x24,node)
    origin=c.get('origin',[0,0,0]);write(node+0x20,struct.pack('<3f',*origin));write(node+(0x74 if pc else 0x70),struct.pack('<3f',*origin))
    if not query:put(obj,int(CATALOG[name][k]['vtable'],16));put(obj+(0x20 if pc else 0x24),owner)
    permission=c.get('permission',1);special=c.get('special',0)
    write(owner+(0x14d if pc else 0x15d),bytes([permission]));write(owner+(0x1c5 if pc else 0x1d5),bytes([special]))
    has_owner=c.get('perceptionOwner',True);put(perception+0x10,owner if has_owner else 0)
    positions=c['targets'];targets=[]
    if pc:put(perception+0x18,sentinel)
    else:sentinel=perception+0x18
    links=[base+0x2800+i*0x500 for i in range(len(positions))];order=[sentinel]+links+[sentinel]
    forward,backward=(0,4) if pc else (4,0)
    for i,a in enumerate(order[:-1]):
        put(a+forward,order[i+1]);put(a+backward,links[-1] if i==0 and links else order[i-1] if i else sentinel)
    for i,(a,position) in enumerate(zip(links,positions)):
        target,te,tn=a+0x80,a+0x180,a+0x280;targets.append(target)
        put(a+8,target);put(target+0x24,te);put(te+0x24,tn);write(tn+0x20,struct.pack('<3f',*position))
    invoked=query or name not in TWO|{'DroidWander'} or bool(permission)
    seen=positions if invoked and has_owner else []
    nearest=None
    if seen:
        distances=[sum((a-b)**2 for a,b in zip(pos,origin)) for pos in seen]
        minimum=min(distances);nearest=max(i for i,d in enumerate(distances) if d==minimum)
    boundary=None;key=None
    if not query:
        if name in ONE and nearest is None:key=1
        elif name in ZERO and nearest is None:key=0
        elif name=='Attack' and nearest is None and not special:key=0
        elif name in TWO and permission and nearest is not None:key=2
        elif name=='DroidWander' and permission and nearest is not None:boundary='visibility'
        if key is not None:boundary='selector'
    camera=[20,30,40]
    if name=='DroidWander':
        manager,view,cn=base+0x5000,base+0x5400,base+0x5800;global_address=0x765af0 if pc else 0x49fc78
        if not pc:p.map(global_address,4)
        put(global_address,manager);put(manager+0x28,view);put(view+0x18,cn);write(cn+(0x74 if pc else 0x70),struct.pack('<3f',*camera))
    before=read(base,0x6000);expected=bytearray(before)
    if seen:expected[0x1830:0x183c]=struct.pack('<3f',*seen[-1])
    def observe(machine,address,size,user):
        if address==(0x4f1910 if pc else 0x36ebc0):calls.append(p.reg('ECX' if pc else 'A0'))
    hook=u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_CODE,observe,begin=0x4f1910 if pc else 0x36ebc0,end=0x4f1910 if pc else 0x36ebc0)
    stop=(0x591c80 if pc else 0x225df0) if boundary=='selector' else (0x597900 if pc else 0x291980) if boundary=='visibility' else None
    if pc:
        assert p.uint(0x702708+0x50)==0x591c80
        p.run(entry,this=perception if query else obj,stop_at=stop)
        result=dict(entry=f'{entry:08X}',stop=f'{p.reg("EIP"):08X}',completion='original return' if stop is None else 'actual consumer entry;guest discarded',blocks=sum(p.visits.values()))
    else:
        if not query:assert p.uint(0x496780+0x58)==0x225df0
        p.reg('A0',perception if query else obj);original=[p.read(a,n) for a,n in p.ranges]
        result=p.run(entry,[stop or p.RETURN],count=6000,timeout_us=500000);assert original==[p.read(a,n) for a,n in p.ranges]
        if stop is None:
            assert p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN
            assert [p.reg(str(i)) for i in range(16,21)]==[0x12340000+i for i in range(16,21)]
            assert [p.reg('F'+str(i))&0xffffffff for i in range(20,24)]==[0x3f800000+i*0x10000 for i in range(20,24)]
            assert result['initialUpper64']==result['finalUpper64']
    u.hook_del(hook);assert calls==([perception] if invoked else []),(calls,invoked)
    details=None
    if query:
        value=p.reg('EAX' if pc else 'V0')&0xffffffff
        assert value==(0 if nearest is None else targets[nearest]),(value,nearest,targets)
        details=dict(nearestIndex=nearest,returnedAddress=value)
    elif boundary=='selector':
        details=dict(receiver=p.reg('ECX' if pc else 'A0'),key=p.uint(p.reg('ESP')+4) if pc else p.reg('A1')&0xffffffff,parameter=p.uint(p.reg('ESP')+8) if pc else p.reg('A2')&0xffffffff)
        assert details==dict(receiver=owner,key=key,parameter=0),details
    elif boundary=='visibility':
        first=p.reg('ESP')+4 if pc else p.reg('A0');second=p.reg('ESP')+16 if pc else p.reg('A1')
        details=dict(first=list(struct.unpack('<3f',read(first,12))),second=list(struct.unpack('<3f',read(second,12))),thirdBits=p.uint(p.reg('ESP')+28) if pc else p.reg('F12')&0xffffffff)
        assert details==dict(first=[origin[0],origin[1]+50,origin[2]],second=camera,thirdBits=0),details
    after=read(base,0x6000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(boundary=boundary,consumer=details,perceptionCalls=len(calls),guardedBytes=0x6000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),scratchPosition=list(seen[-1]) if seen else None)
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter()
    report=dict(kind='original-perception-and-ai-control',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
