#!/usr/bin/env python3
"""Original four trigger Tick paths through real geometry and consumer boundaries."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/trigger-tick-gates'
SPECS={
 'wxChangeCharacterPlacement':dict(pc=0x540eb0,ps2=0x39bf50,end=0x39c000,v19=0x541040,v20=0x48eaa0,v21=0x540f20,v22=0x48eaa0,ps19=0x39bd00,ps20=0x39bda0),
 'wxOpeningGate':dict(pc=0x537570,ps2=0x3b2a20,end=0x3b2a80,v19=0x537650,v20=0x5377e0,v21=0x48eaa0,v22=0x48eaa0,ps19=0x3b25b0,ps20=0x3b27e0),
 'wxPivotingDoor':dict(pc=0x539b40,ps2=0x3b3de0,end=0x3b3eb8,v19=0x539c60,v20=0x53a1b0,v21=0x539bc0,v22=0x53d360,ps19=0x3b3cd0,ps20=0x3b3700,enabled=0x168),
 'wxPushButton':dict(pc=0x53b0b0,ps2=0x3b5ba0,end=0x3b5c78,v19=0x53b250,v20=0x53b2f0,v21=0x53b130,v22=0x53d360,ps19=0x3b5ab0,ps20=0x3b5530,enabled=0x148)}
def sha(v):return hashlib.sha256(v).hexdigest().upper()

def execute(c):
    pc=c['platform']=='pc';cls=c['className'];s=SPECS[cls]
    if pc:
        p=PcBlocks();base=p.allocate(0x9000);write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
    else:
        vt=int(c['vtable'],16);p=Ps2ScalarPrefix([(s['ps2'],s['end']-s['ps2']+4),(0x28f020,8),(vt,0x70)])
        base=0x21000000;p.map(base,0x9000);p.map(0x49f000,4096);p.map(0x22000000,4096);p.reg('GP',0x4a4170);p.reg('SP',0x22000800)
        write,read,put=p.write,p.read,p.put_uint
    obj,profile,player,own_node,player_node,timer=base,base+0x1000,base+0x4000,base+0x5000,base+0x6000,base+0x7000
    write(base,b'\xa5'*0x9000);put(obj,int(c['vtable'],16));put(obj+0x18,own_node)
    put(0x765ad4 if pc else 0x49fc7c,profile);put(profile+0x2b4,player);put(player+0x24,player_node)
    put(0x755298 if pc else 0x49fc80,timer);write(timer+0x40,bytes([c['paused']]));put(timer+0x1c,c['now'])
    write(obj+(0x126 if pc else 0x132),bytes([c['active']]));put(obj+(0x13c if pc else 0x148),c['previous']);put(obj+(0x140 if pc else 0x14c),c['interval']);put(obj+(0x128 if pc else 0x134),25)
    if 'enabled' in s:
        off=s['enabled']+(0 if pc else 24);write(obj+off,bytes([c['enabled'],c['queued']]))
    write(own_node+(0x74 if pc else 0x70),struct.pack('<3f',c['distance'],0,0));write(player_node+(0x74 if pc else 0x70),struct.pack('<3f',0,0,0))
    write(player_node+(0xa4 if pc else 0xa8),struct.pack('<3f',c['axis'],0,0));write(own_node+(0xa4 if pc else 0xa8),struct.pack('<3f',-1,0,0));write(obj+(0x190 if pc else 0x1a8),struct.pack('<3f',c['distance'],0,0))
    before=read(base,0x9000);expected=bytearray(before);due=((c['now']-c['previous'])&0xffffffff)>c['interval'];square=c['distance']**2
    near=square<=25 if cls=='wxPivotingDoor' else square<25
    if cls in ('wxPivotingDoor','wxPushButton'):near=near and not c['queued']
    if cls=='wxPushButton':near=near and c['distance']!=0 and c['axis']>.7
    callback=None;predicate_calls=0;cooldown_calls=0;value=int(not c['paused'])
    if not c['paused']:
        if cls=='wxOpeningGate':callback='v20' if c['active'] else None
        elif cls=='wxChangeCharacterPlacement':
            cooldown_calls=1
            if due:
                predicate_calls=1
                if not c['active'] and near:callback='v21'
                elif c['active'] and not near:expected[0x126]=0
        elif c['enabled']:
            if c['active']:
                predicate_calls=1
                if not near:callback='v22'
            else:
                cooldown_calls=1
                if due:
                    predicate_calls=1
                    if near:callback='v21'
            if callback is None and c['queued']:callback='v20'
    if pc and cooldown_calls and due:struct.pack_into('<I',expected,0x13c,c['now'])
    if pc:
        observed=[]
        for a in (0x5968f0,0x590330,s['v19']):
            def observe(mu,ip,n,user):observed.append(f'{ip:08X}')
            p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=a,end=a)
        stop=s[callback] if callback else None;p.run(s['pc'],this=obj,stop_at=stop)
        assert observed.count('005968F0')==1
        assert observed.count('00590330')==cooldown_calls
        assert observed.count(f'{s["v19"]:08X}')==predicate_calls
        if stop:assert p.reg('ECX')==obj
        else:assert p.reg('EAX')&255==value
        result=dict(entry=f'{s["pc"]:08X}',stop=f'{p.reg("EIP"):08X}',completion='actual '+callback+' entry;guest discarded' if stop else 'whole original Tick return',callback=callback,returnValue=None if stop else value,predicateCalls=predicate_calls,cooldownCalls=cooldown_calls,observedEntries=observed,blocks=sum(p.visits.values()))
    else:
        # Only initial original component;the first actual game consumer ends
        # it. No cooldown or geometry return is invented to join PS2 frames.
        expected=bytearray(before);boundary=None;stop=s['end']
        if not c['paused']:
            if cls=='wxOpeningGate':
                if c['active']:boundary='v20';stop=s['ps20']
            elif cls=='wxChangeCharacterPlacement':boundary='cooldown';stop=0x3ac940
            elif c['enabled']:
                boundary='v19' if c['active'] else 'cooldown';stop=s['ps19'] if c['active'] else 0x3ac940
        entry=s['ps2']+12;p.reg('A0',obj);original=[read(a,n) for a,n in p.ranges];result=p.run(entry,[stop],timeout_us=500000);assert original==[read(a,n) for a,n in p.ranges]
        assert p.trace.count(0x28f020)==1
        if boundary:assert p.reg('A0')==obj
        else:assert p.reg('V0')&0xffffffff==value
        result.update(boundary=boundary,returnValue=None if boundary else value,completion='own post-SQ component at '+boundary if boundary else 'own post-SQ component at final return-register restore')
    after=read(base,0x9000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(guardedBytes=0x9000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),effects=[dict(offset=f'{i:X}',before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b],scope='Borrowed cached timer/profile/player/Nodes and original vtables. PC real cooldown/predicates;only callbacks stop before execution. PS2 initial component only,no fake callee returns.')
    return result

def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    rows=[c for c in json.loads((FOLDER/'tick-cases.json').read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='original-trigger-tick-gates',status='running',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha((FOLDER/'tick-cases.json').read_bytes()),cases=[]);started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in rows:
        row=dict(input=c);report['cases'].append(row);save()
        try:row.update(status='passed',**execute(c))
        except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save();print(json.dumps(dict(status=report['status'],cases=len(rows),failed=failed,seconds=report['seconds'])));return int(failed!=0)

if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
