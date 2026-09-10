#!/usr/bin/env python3
"""Original AIAction input fields,real PC registry and separate PS2 components.

PS2 registry and caller suffix use fresh guests with explicit input/output
bindings. This does not execute a nested SQ/LQ call or synthesize its return.
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
FIELDS={
 'DarcyAttack':dict(zeroWords=[0x3b0,0x3b8,0x3c0,0x3b4,0x3cc,0x400],zeroBytes=[0x3ac,0x3f0,0x45c],oneWords=[0x3bc],twoWords=[0x3d0],oneBytes=[],ps2Start=0x2487b0,ps2Stop=0x2487fc),
 'IcyAttack':dict(zeroWords=[0x3b0,0x3b8,0x3c0,0x3b4,0x3c4,0x410],zeroBytes=[0x3ac,0x3ad,0x3e8,0x3e9,0x3eb],oneWords=[0x3bc],twoWords=[0x3c8],oneBytes=[0x468],ps2Start=0x2591f0,ps2Stop=0x259248),
 'StormyAttack':dict(zeroWords=[0x3b0,0x3b8,0x3c4,0x3b4,0x3c8,0x3d4],zeroBytes=[0x3ac,0x3f4],oneWords=[0x3c0],twoWords=[0x3d8],oneBytes=[0x3d0],ps2Start=0x2665a0,ps2Stop=0x2665ec),
 'WinxFly':dict(zeroWords=[0x3ac,0x3b8,0x3b0,0x3b4],zeroBytes=[0x3bc],oneWords=[],twoWords=[],oneBytes=[],ps2Start=0x26d0a8,ps2Stop=0x26d0d4),
}


def selector_component(c):
    p=Ps2ScalarPrefix([(0x369b60,0x5c)]);area=0x21000000;p.map(area,0x1000);p.map(0x22000000,0x1000);p.reg('SP',0x22000800)
    p.write(area,b'\xa5'*0x1000);p.put_uint(area+0x24,c['target']);p.put_uint(area+0x18,0)
    before=p.read(area,0x1000);p.reg('A0',area);r=p.run(0x369b6c,[0x369bb0],timeout_us=500000)
    assert p.reg('V0')&0xffffffff==c['target'] and p.read(area,0x1000)==before
    r.update(originalEntry='00369B60',outputV0=p.reg('V0')&0xffffffff,scope='Borrowed cached target or count0;no index helper or nested SQ/LQ execution',storageSha256=hashlib.sha256(before).hexdigest().upper())
    return r


def execute(row,c):
    name=row['className'][2:-8];ispc=c['platform']=='pc';delta=0 if ispc else 4;shared=name in FIELDS;parts=[]
    if not ispc and shared:parts.append(selector_component(c))
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;area=p.allocate(0x4000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        entry=int(row['ps2']['slots'][10],16);p=Ps2ScalarPrefix([(entry,0x100)]);area=0x21000000;p.map(area,0x4000);p.map(0x22000000,0x1000);p.reg('SP',0x22000800)
        read,write,put=p.read,p.write,p.put_uint
    write(area,bytes([c['fill']])*0x4000)
    obj,owner,char,cmd,entity,node,registry=area,area+0x800,area+0xc00,area+0x1000,area+0x1400,area+0x1800,area+0x1c00
    put(obj,int(row[c['platform']]['vtable'],16));put(obj+0x20+delta,owner);put(owner+(0x144 if ispc else 0x154),char)
    command_present=c.get('commandPresent',True);put(char+(0x130 if ispc else 0x13c),cmd if command_present else 0)
    put(owner+0x24,entity);put(entity+0x24,node)
    position=[-1.5,0.,2.5] if c['fill']==0xa5 else [3.25,-4.,0.]
    write(node+0x20,struct.pack('<3f',*position))
    if ispc and shared:
        put(0x765ad8,registry);put(registry+0x28,c['target']);put(registry+0x18,0)
    before=read(area,0x4000);expected=bytearray(before)
    def ew(a,v):struct.pack_into('<I',expected,a-area,v&0xffffffff)
    def eb(a,v):expected[a-area]=v
    if name=='Help':
        assert command_present;ew(cmd+4,0);eb(obj+0x3ac+delta,0);ew(obj+0x460+delta,0)
    elif name=='GhoulScript':
        ew(obj+0x3b0+delta,node);ew(obj+0x3b8+delta,cmd if command_present else 0);eb(owner+(0x14d if ispc else 0x15d),0)
        ew(obj+0x3a8+delta,0);ew(obj+0x3ac+delta,0);ew(obj+0x370+delta,15000)
    elif name=='Wander':
        ew(obj+0x3b8+delta,0);ew(obj+0x3bc+delta,0);expected[0x3a8+delta:0x3b4+delta]=struct.pack('<3f',*position)
    elif shared:
        config=FIELDS[name];ew(obj+0x3a8+delta,c['target'])
        for k,v,width in [('zeroWords',0,4),('oneWords',1,4),('twoWords',2,4),('zeroBytes',0,1),('oneBytes',1,1)]:
            for off in config[k]:(ew if width==4 else eb)(obj+off+delta,v)
        if name=='WinxFly' and ispc and command_present:ew(cmd+4,0)
    else:raise ValueError(name)
    if ispc:
        observed=[]
        def observe(u,a,n,user):
            if a==0x4e21d0:observed.append(p.reg('ECX'))
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
        entry=int(row['pc']['slots'][10],16);p.run(entry,obj,(c['argument'],))
        assert observed==([registry] if shared else [])
        result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()),registryCalls=observed)
    else:
        if shared:
            config=FIELDS[name];assert parts[0]['outputV0']==c['target']
            p.reg('S0',obj);p.reg('V0',parts[0]['outputV0']);result=p.run(config['ps2Start'],[config['ps2Stop']],timeout_us=500000)
            result.update(originalEntry=row['ps2']['slots'][10],componentInput=dict(S0=obj,V0=parts[0]['outputV0']),scope='Caller own suffix after registry call;input V0 obtained by separate original registry component. No nested-call execution claim.')
        else:
            p.reg('A0',obj);p.reg('A1',c['argument']);result=p.run(entry,[p.RETURN],timeout_us=500000)
    after=read(area,0x4000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:16]
    result.update(borrowedStorageBytes=0x4000,beforeSha256=hashlib.sha256(before).hexdigest().upper(),afterSha256=hashlib.sha256(after).hexdigest().upper(),changes=[dict(offset=i,before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b],supportComponents=parts)
    return result


def guest(output,selection):
    output=Path(output).resolve();selection=Path(selection).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    cases=json.loads(selection.read_text())['cases'];assert 0<len(cases)<=100;rows={r['className']:r for r in json.loads(CATALOG.read_text())['classes']}
    started=time.perf_counter();report=dict(kind='original-ai-action-entry-fields',status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selectionSha256=hashlib.sha256(selection.read_bytes()).hexdigest().upper(),cases=[])
    output.parent.mkdir(parents=True,exist_ok=True)
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in cases:
        report['pending']=c;save()
        try:result=dict(input=c,status='passed',**execute(rows[c['className']],c))
        except Exception as error:result=dict(input=c,status='blocked',error=str(error),traceback=traceback.format_exc())
        report['cases'].append(result);report.pop('pending');save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='passed' if not failed else 'partial';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
