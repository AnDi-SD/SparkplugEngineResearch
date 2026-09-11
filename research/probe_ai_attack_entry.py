"""Original attack entry fields, full perception and known consumer boundaries."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
import probe_pc_animation_lifecycle as lifetime

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ai-attack-entry'
CATALOG={c['className'][2:-8]:c for c in json.loads((ROOT/'research/ai-action-construction-contracts-2026-09-10.json').read_text())['classes']}
CAMERA={'IceGargoyleAttack','IceGargoyleClaw','IceGargoyleWithdrawl'}
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';name=c['name'];entry=int(CATALOG[name][k]['slots'][10],16);calls=[]
    if pc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;base=p.allocate(0x6000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        p=Ps2ScalarPrefix([(entry,0x180),(0x36ebc0,0x180),(0x14e640,0x28),(0x369b60,0x60),(0x293210,0xd0),(0x423b58,0x28),(0x109af0,0x14)],profile='integer-squares',stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,0x6000);p.map(0x22000000,4096);read,write,u=p.read,p.write,p.u;p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        for i in range(16,21):p.reg(str(i),0x12340000+i)
        raw,sections=pristine();p.map(0x496780,0x6c);write(0x496780,read_window('ps2',raw,0x496780,0x6c,sections)[0])
    obj,owner,char,cmd,perception,entity,node,sentinel,link,target,te,tn,config,move,registry,cam,view,cn=[base+x for x in (0,0x1000,0x1400,0x1800,0x1c00,0x2000,0x2400,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x3000,0x3800,0x4000,0x4400,0x4800,0x4c00)]
    put=p.put_uint;write(base,bytes([c['fill']])*0x6000);delta=0 if pc else 4
    put(obj,int(CATALOG[name][k]['vtable'],16));put(obj+0x20+delta,owner);put(owner,0x702708 if pc else 0x496780)
    put(owner+(0x144 if pc else 0x154),char);put(char+(0x130 if pc else 0x13c),cmd);put(char+(0x154 if pc else 0x160),perception)
    put(char+(0x138 if pc else 0x144),config);put(char+(0x12c if pc else 0x138),move)
    put(owner+0x24,entity);put(entity+0x24,node);put(target+0x24,te);put(te+0x24,tn)
    origin=[0,0,0];position=c.get('position',[3,4,0]);has_target=c['target']
    for a,pos in [(node,origin),(tn,position)]:
        write(a+0x20,struct.pack('<3f',*pos));write(a+(0x74 if pc else 0x70),struct.pack('<3f',*pos))
    put(perception+0x10,owner)
    if pc:put(perception+0x18,sentinel)
    else:sentinel=perception+0x18
    forward,backward=(0,4) if pc else (4,0)
    put(sentinel+forward,link if has_target else sentinel);put(sentinel+backward,link if has_target else sentinel)
    put(link+forward,sentinel);put(link+backward,sentinel);put(link+8,target)
    if name=='BacoAttack':
        g=0x765ad8 if pc else 0x49fd88
        if not pc:p.map(g,4)
        put(g,registry);put(registry+(0x28 if pc else 0x24),target if has_target else 0);put(registry+0x18,0)
    if name in CAMERA:
        g=0x765af0 if pc else 0x49fc78
        if not pc:p.map(g,4)
        put(g,cam);put(cam+0x28,0 if c.get('nullView') else view);put(view+0x18,cn if has_target else 0)
    if name=='FrogAttack':write(config+(0x138 if pc else 0x144),struct.pack('<f',c['multiplier']))
    selected=[10.,20.,30.];index=c.get('configIndex',0)
    if name=='IceWormAttack':
        write(obj+0x3b8+delta,struct.pack('<3f',c['initialX'],-8.,9.));put(config+(0x188 if pc else 0x194),index)
        write(config+(0x13c if pc else 0x148)+index*12,struct.pack('<3f',*selected))
    before=read(base,0x6000);expected=bytearray(before)
    def ew(a,v):struct.pack_into('<I',expected,a-base,v&0xffffffff)
    def eb(a,v):expected[a-base]=v
    def ef(a,v):struct.pack_into('<f',expected,a-base,v)
    target_value=(cn if name in CAMERA else target) if has_target and not c.get('nullView') else 0
    sensor=name not in CAMERA and name!='BacoAttack'
    if sensor and has_target:expected[perception-base+0x30:perception-base+0x3c]=struct.pack('<3f',*position)
    if name in CAMERA:
        if name=='IceGargoyleAttack':
            ew(obj+0x3d8+delta,target_value);ew(obj+0x3dc+delta,2);ew(obj+0x3e0+delta,0);eb(obj+0x3b4+delta,0)
        else:ew(obj+0x3ac+delta,target_value);ew(obj+0x3b0+delta,1);ew(obj+0x3b4+delta,0)
    else:
        eb(cmd+0x1d,1);ew(obj+0x3a8+delta,target_value);ew(obj+0x3ac+delta,4 if name=='BacoAttack' else 0);ew(obj+0x3b0+delta,0)
        if name=='IceWormAttack':
            if c['initialX']<1:expected[0x3b8+delta:0x3c4+delta]=struct.pack('<3f',*selected)
        else:
            eb(obj+0x3bc+delta,0)
            if name=='BacoAttack':ew(obj+0x3b4+delta,0x47afc800);ew(obj+0x354+delta,0)
            elif name=='FrogAttack':ew(obj+0x3e8+delta,cmd);ef(obj+0x3b4+delta,c['multiplier']*160000.)
            else:
                if name=='DroidAttack':
                    for off in (0x1c,0x20,0x21):eb(cmd+off,0)
                    value=0x47742400
                else:value=0 if has_target and sum(v*v for v in position)<24000 else 0x481c4000
                ew(obj+0x3b4+delta,value)
                if has_target:
                    eb(perception+0x70,0);ew(perception+0x74,0x7f7fffff);eb(perception+0x78,1)
                    if name=='ShadowBeastAttack':ew(obj+0x3dc+delta,move);ew(move+(0x1e0 if pc else 0x1ec),tn)
    boundary=target_value==0;key=0 if name in CAMERA|{'IceWormAttack'} else 1
    watch={0x4f1910 if pc else 0x36ebc0:'perception',0x4e21d0 if pc else 0x369b60:'registry',0x597660 if pc else 0x293210:'distance'}
    def observe(machine,address,size,user):
        if address in watch:calls.append(watch[address])
    hooks=[u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_CODE,observe,begin=a,end=a) for a in watch]
    stop=(0x591c80 if pc else 0x225df0) if boundary else None
    if pc:
        assert p.uint(0x702708+0x50)==0x591c80;p.run(entry,this=obj,args=(c['argument'],),stop_at=stop)
        result=dict(entry=f'{entry:08X}',completion='actual selector boundary;guest discarded' if boundary else 'original return',blocks=sum(p.visits.values()))
    else:
        assert p.uint(0x496780+0x58)==0x225df0;p.reg('A0',obj);p.reg('A1',c['argument']);original=[read(a,n) for a,n in p.ranges]
        result=p.run(entry,[stop or p.RETURN],count=6000,timeout_us=500000);assert original==[read(a,n) for a,n in p.ranges]
        if not boundary:
            assert p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN
            assert [p.reg(str(i)) for i in range(16,21)]==[0x12340000+i for i in range(16,21)]
            assert result['initialUpper64']==result['finalUpper64']
    for hook in hooks:u.hook_del(hook)
    expected_calls=['registry'] if name=='BacoAttack' else ['perception'] if sensor else []
    if name in ('SpiderAttack','ShadowBeastAttack') and has_target:expected_calls.append('distance')
    assert calls==expected_calls,(calls,expected_calls)
    consumer=None
    if boundary:
        consumer=dict(receiver=p.reg('ECX' if pc else 'A0'),key=p.uint(p.reg('ESP')+4) if pc else p.reg('A1')&0xffffffff,parameter=p.uint(p.reg('ESP')+8) if pc else p.reg('A2')&0xffffffff)
        assert consumer==dict(receiver=owner,key=key,parameter=0),consumer
    after=read(base,0x6000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(boundary='selector' if boundary else None,consumer=consumer,originalCalls=calls,guardedBytes=0x6000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)))
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot') and c.get('platforms',('pc','ps2'))];start=time.perf_counter()
    report=dict(kind='original-ai-attack-entry',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
