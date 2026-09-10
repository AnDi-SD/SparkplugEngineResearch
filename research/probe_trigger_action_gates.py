#!/usr/bin/env python3
"""Original Dispel/Chest gates; full PC empty-subscription dispatch, explicit PS2 pieces."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/trigger-action-gates'
def sha(v):return hashlib.sha256(v).hexdigest().upper()
def signed(v):return v-(1<<32) if v&0x80000000 else v

def execute(c):
    pc=c['platform']=='pc';kind=c['kind'];stage=c.get('stage');shift=0 if pc else 24
    if pc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;base=p.allocate(0x9000);write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(0x3a8030,0x280),(0x39d540,0x10),(0x339260,0x58),(0x497220,0x70)])
        base=0x21000000;p.map(base,0x9000);p.map(0x49f000,4096);p.map(0x22000000,4096)
        write,read,put=p.write,p.read,p.put_uint;p.reg('GP',0x4a4170);p.reg('SP',0x22000800)
    obj,manager,char,command,machine,profile,flow=base,base+0x1000,base+0x2000,base+0x3000,base+0x3800,base+0x4000,base+0x8000
    write(base,b'\xa5'*0x9000);put(obj,0x6fd630 if pc else 0x497220)
    put(0x765ad8 if pc else 0x49fd88,manager);put(manager+(0x28 if pc else 0x24),char)
    put(char+(0x130 if pc else 0x13c),command);put(char+(0x124 if pc else 0x130),machine)
    write(command+0x1e,bytes([c['command']]));put(machine+(0x130 if pc else 0x13c),c['state'])
    put(0x765ad4 if pc else 0x49fc7c,profile);write(profile+0x2cb4,bytes([c['profileAction']]));write(profile+0x2cb6,bytes([c['profileTick']]))
    put(0x755294 if pc else 0x49fd4c,flow);put(flow+0x1b0,c['gameState'])
    for i,v in enumerate(c['queue']):put(flow+0x1b8+12*i,v)
    write(obj+0x1b0+shift,bytes([c['active']]));write(obj+0x1b8+shift,bytes([c['completed']]))
    put(obj+0x1b4+shift,c['counter']);put(obj+0x1bc+shift,c['selector'])
    # Declared valid short GenericTrigger parent path. Original v17 is retained.
    write(obj+(0x125 if pc else 0x131),b'\x01');write(obj+(0x127 if pc else 0x133),b'\x01')
    put(obj+(0x398 if pc else 0x3b0),c['chestWord'])
    tick_action=not c['completed'] and c['active'] and c['command'] and c['profileTick']
    gate2=not c['state'] and (c['profileAction'] or c['selector']==6) and 70 not in c['queue'] and not c['queue'][0] and signed(c['gameState'])<37
    emits=bool(c['command'] and gate2 and (kind=='action' or kind=='tick' and tick_action))
    subscriptions=None;events=[];visits=[]
    if pc and kind in ('tick','action'):
        subscriptions=f.call(0x4165b0);put(0x75537c,subscriptions)
        assert p.uint(subscriptions+0x1c)==0
        subscription_before=read(subscriptions,0x20);retained={a for a in f.allocations if a not in f.freed}
    before=read(base,0x9000);expected=bytearray(before)
    if kind=='enable':expected[0x1b0+shift]=int(c['slot']==21)
    if pc and emits or not pc and kind=='counter':struct.pack_into('<I',expected,0x1b4+shift,(c['counter']+1)&0xffffffff)
    if pc:
        entry={'enable':0x549af0 if c['slot']==21 else 0x549b00,'chest':0x537b80,'tick':0x549ce0,'action':0x549d50}[kind]
        def observe(mu,a,n,user):
            visits.append(f'{a:08X}')
            if a==0x415a20:
                msg=p.uint(p.reg('ESP')+4);events.append([p.uint(msg+i) for i in range(0,32,4)])
        for a in (0x549d50,0x5904d0,0x4f3df0,0x415a20):p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=a,end=a)
        f.call(entry,this=obj)
        if kind=='chest':assert p.reg('EAX')&255==int(c['chestWord']==0)
        if kind=='tick':
            assert p.reg('EAX')&255==1
            assert visits.count('00549D50')==int(bool(tick_action))
            assert visits.count('005904D0')==int(not c['completed'])
            assert visits.count('004F3DF0')==int(not c['completed'])
        assert events==([[0x2807,0,0,0x22,obj,0,obj,c['counter']]] if emits else []),events
        result=dict(entry=f'{entry:08X}',completion='whole original PC method;actual empty-subscription message dispatch when eligible',blocks=sum(p.visits.values()),events=events,observedEntries=visits)
        if subscriptions:
            assert read(subscriptions,0x20)==subscription_before
            assert {a for a in f.allocations if a not in f.freed}==retained
            result['subscriptionGuard']=dict(groupCount=0,retainedAllocations=len(retained),temporaryAllocationsFreed=True)
    else:
        regs=dict(A0=obj);value=None;boundary=None
        if kind=='enable':entry=0x3a8290 if c['slot']==21 else 0x3a8280;stop=p.RETURN
        elif kind=='chest':entry=0x39d540;stop=p.RETURN;value=int(c['chestWord']==0)
        elif kind=='tick' and stage=='head':
            entry=0x3a81cc
            if c['completed']:stop=0x3a826c;value=1
            elif not c['active']:stop=0x3ac9d0;boundary='parent'
            else:stop=0x369b60;boundary='cached-character-lookup'
        elif kind=='tick' and stage=='after-lookup':
            entry=0x3a81f8;regs=dict(S0=obj,V0=char)
            stop=0x3a8030 if c['command'] and c['profileTick'] else 0x3ac9d0;boundary='action' if stop==0x3a8030 else 'parent'
        elif kind=='action' and stage=='head':entry=0x3a803c;stop=0x369b60;boundary='cached-character-lookup'
        elif kind=='action' and stage=='after-first-lookup':
            entry=0x3a8050;regs=dict(S0=obj,V0=char);stop=0x369b60 if c['command'] else 0x3a81a4
            boundary='cached-character-lookup' if c['command'] else 'own-exit'
        elif kind=='action' and stage=='after-second-lookup':
            entry=0x3a8074;regs=dict(S0=obj,V0=char);stop=0x1007a0 if gate2 else 0x3a81a4;boundary='message' if gate2 else 'own-exit'
        elif kind=='counter':entry=0x3a8130;regs=dict(S0=obj);stop=0x3a813c;boundary='post-message-counter-component'
        else:raise ValueError((kind,stage))
        for name,v in regs.items():p.reg(name,v)
        original_code=[p.read(a,n) for a,n in p.ranges]
        result=p.run(entry,[stop],count=6000,timeout_us=500000)
        assert [p.read(a,n) for a,n in p.ranges]==original_code
        if value is not None:assert p.reg('V0')&0xffffffff==value
        if boundary=='cached-character-lookup':assert p.reg('A0')==manager
        if boundary in ('action','parent'):assert p.reg('A0')==obj
        if boundary=='message':
            args=[p.reg(x)&0xffffffff for x in ('A0','A1','A2','A3','T0')]
            assert args==[obj,0x2807,0x22,obj,c['counter']],args;result['messageArguments']=args
        result.update(initialRegisters=regs,boundary=boundary,scope='Each component is a fresh guest. Borrowed cached-character output declared as input;SQ lookup/frame/message bodies and parent Tick are not executed.')
    after=read(base,0x9000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:16]
    result.update(guardedBytes=0x9000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),counterBefore=c['counter'],counterAfter=p.uint(obj+0x1b4+shift),effects=[dict(offset=f'{i:X}',before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b])
    if subscriptions:
        f.call(p.uint(p.uint(subscriptions)),this=subscriptions,args=(1,));assert set(f.allocations)==set(f.freed);result['subscriptionTeardown']='all original manager allocations released'
    return result

def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    rows=[c for c in json.loads((FOLDER/'action-gate-cases.json').read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='original-trigger-action-gates',status='running',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha((FOLDER/'action-gate-cases.json').read_bytes()),cases=[]);started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in rows:
        row=dict(input=c);report['cases'].append(row);save()
        try:row.update(status='passed',**execute(c))
        except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save();print(json.dumps(dict(status=report['status'],cases=len(rows),failed=failed,seconds=report['seconds'])));return int(failed!=0)

if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
