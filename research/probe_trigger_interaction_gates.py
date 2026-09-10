#!/usr/bin/env python3
"""Original Pivot/Push/Snow predicates; PS2 floating-point boundaries stay explicit."""
import hashlib,json,math,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/trigger-interaction-gates'
SPECS={'wxPivotingDoor':dict(pc=0x539c60,ps2=0x3b3cd0,end=0x3b3dc4,flag=0x169),
       'wxPushButton':dict(pc=0x53b250,ps2=0x3b5ab0,end=0x3b5b90,flag=0x149),
       'wxSnowPile':dict(pc=0x54b740,ps2=0x3c0f80,end=0x3c0fcc,flag=0x124)}
def sha(v):return hashlib.sha256(v).hexdigest().upper()
def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]

def execute(c):
    pc=c['platform']=='pc';cls=c['className'];spec=SPECS[cls]
    if pc:
        p=PcBlocks();base=p.allocate(0x8000);write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
    else:
        ranges=[(spec['ps2'],spec['end']-spec['ps2']+4)]
        if cls=='wxPushButton':ranges += [(0x293210,0xd0),(0x109af0,0x14),(0x423b58,0x28)]
        p=Ps2ScalarPrefix(ranges,profile='integer-squares' if cls=='wxPushButton' else 'r4000');base=0x21000000
        p.map(base,0x8000);p.map(0x49f000,4096);p.map(0x22000000,4096);p.reg('GP',0x4a4170);p.reg('SP',0x22000800)
        write,read,put=p.write,p.read,p.put_uint
    obj,profile,player,player_node,own_node=base,base+0x1000,base+0x4000,base+0x5000,base+0x6000
    write(base,b'\xa5'*0x8000);put(obj,int(c['vtable'],16));put(obj+0x18,own_node)
    put(0x765ad4 if pc else 0x49fc7c,profile);put(profile+0x2b4,player);put(player+0x24,player_node);put(profile+0x6f8,c['gameMode'])
    offset=spec['flag']+(0 if pc else (12 if cls=='wxSnowPile' else 24))
    write(obj+offset,bytes([c['flag']]));put(obj+(0x128 if pc else 0x134),c['threshold'])
    for node,position,axis in [(own_node,c['ownPosition'],c['ownAxis']),(player_node,c['playerPosition'],c['playerAxis'])]:
        write(node+(0x74 if pc else 0x70),struct.pack('<3f',*position));write(node+(0xa4 if pc else 0xa8),struct.pack('<3f',*axis))
    write(obj+(0x190 if pc else 0x1a8),struct.pack('<3f',*c['cachedPosition']))
    before=read(base,0x8000);calls=[]
    if cls=='wxPivotingDoor':
        delta=[f32(a-b) for a,b in zip(c['cachedPosition'],c['playerPosition'])];dy=delta[1]
        if abs(dy)<200:delta[1]=0
        square=sum(v*v for v in delta);expected=int(not c['flag'] and square<=c['threshold'])
    else:
        delta=[f32(a-b) for a,b in zip(c['ownPosition'],c['playerPosition'])]
        if cls=='wxPushButton' or abs(delta[1])<100:delta[1]=0
        square=sum(v*v for v in delta)
        if cls=='wxPushButton':
            inside=not c['flag'] and square<c['threshold']
            length=math.sqrt(square);normalized=[f32(v/length) if length>.001 else 0.0 for v in delta]
            dot=f32(sum(f32(a*b) for a,b in zip(normalized,c['playerAxis'])))
            expected=int(inside and dot>f32(.7))
        else:expected=int(c['gameMode']!=10 and c['flag']!=0 and square<c['threshold'])
    if pc:
        for a in (0x597660,0x597220,0x590640):
            def observe(mu,ip,n,user):
                row=dict(entry=f'{ip:08X}')
                if ip in (0x597660,0x597220):row['args']=[p.uint(p.reg('ESP')+i*4) for i in range(1,6 if ip==0x597220 else 5)]
                calls.append(row)
            p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=a,end=a)
        p.run(spec['pc'],this=obj);value=p.reg('EAX')&255;assert value==expected,(value,expected,square,c)
        if cls=='wxPushButton':
            expected_calls=[]
            if not c['flag']:expected_calls.append(dict(entry='00597660',args=[player_node,own_node,1,0]))
            if inside:expected_calls.append(dict(entry='00597220',args=[own_node,player_node,bits(.7),1,0]))
            assert calls==expected_calls,(calls,expected_calls)
        if cls=='wxSnowPile':assert [x['entry'] for x in calls].count('00590640')==int(c['gameMode']!=10)
        result=dict(entry=f'{spec["pc"]:08X}',completion='whole original PC method and reached helpers',returnValue=value,blocks=sum(p.visits.values()),calls=calls)
    else:
        entry=spec['ps2']+12;p.reg('A0',obj)
        if cls=='wxPivotingDoor':stop=spec['end'] if c['flag'] else 0x3b3d20
        elif cls=='wxPushButton':stop=0x292190 if inside else spec['end']
        else:stop=spec['end'] if c['gameMode']==10 else 0x3ac890
        original=[read(a,n) for a,n in p.ranges];result=p.run(entry,[stop],count=6000,timeout_us=500000);assert original==[read(a,n) for a,n in p.ranges]
        if cls=='wxPivotingDoor' and not c['flag']:
            # Stop before CVT.W.S/MOVZ/conditional-delay MADD. No rounding or
            # absolute-height result is supplied or claimed for PS2.
            assert p.reg('F2')&0xffffffff==bits(dy)
            assert p.reg('F3')&0xffffffff==bits(c['playerPosition'][0])
            assert p.reg('F4')&0xffffffff==bits(c['cachedPosition'][0])
            assert p.reg('F5')&0xffffffff==bits(c['playerPosition'][2])
            result.update(completion='own original coordinate prelude before integer conversion;PS2 result unexecuted',heightDifferenceBits=f'{bits(dy):08X}')
        elif cls=='wxPushButton' and inside:
            assert [p.reg(x) for x in ('A0','A1','A2','A3')]==[own_node,player_node,1,0]
            assert p.reg('F12')&0xffffffff==bits(.7)
            assert len(result['accumulatorInterpretation'])==3
            result.update(completion='original distance result and actual orientation-call arguments;SQRT body not executed',orientationArgs=dict(first='own',second='player',thresholdBits=f'{bits(.7):08X}',ignoreY=1,negate=0))
        elif cls=='wxSnowPile' and c['gameMode']!=10:
            assert p.reg('A0')==obj;result['completion']='original mode gate at inherited GenericTrigger v19 entry'
        else:assert p.reg('V0')&0xffffffff==0;result['returnValue']=0
    after=read(base,0x8000);assert after==before,'whole borrowed records unchanged'
    result.update(guardedBytes=0x8000,beforeSha256=sha(before),afterSha256=sha(after),modelSquaredDistance=square,modelScope='PC predicate expectation;not an unexecuted PS2 result',scope='Explicit cached profile/player/Node/receiver records. No normal binding or complete PS2 SQRT/convert/MOVZ/conditional-MADD claim.')
    return result

def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    rows=[c for c in json.loads((FOLDER/'interaction-cases.json').read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='original-trigger-interaction-gates',status='running',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha((FOLDER/'interaction-cases.json').read_bytes()),cases=[]);started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in rows:
        row=dict(input=c);report['cases'].append(row);save()
        try:row.update(status='passed',**execute(c))
        except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save();print(json.dumps(dict(status=report['status'],cases=len(rows),failed=failed,seconds=report['seconds'])));return int(failed!=0)

if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
