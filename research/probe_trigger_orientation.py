#!/usr/bin/env python3
"""Original PC facing helper/callers and explicit PS2 input/prelude boundaries."""
import hashlib,json,math,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/trigger-runtime-controls'


def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
def sha(v):return hashlib.sha256(v).hexdigest().upper()


def execute(c):
    pc=c['platform']=='pc';kind=c['kind']
    if pc:
        p=PcBlocks();base=p.allocate(0x7000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        entry=0x292190 if kind=='helper' else {23:0x3ac6bc,24:0x3ac65c}[c['slot']]
        p=Ps2ScalarPrefix([(entry,0x58 if kind=='helper' else 0x50)]);base=0x21000000;p.map(base,0x7000);p.map(0x49f000,4096);p.map(0x22000000,4096)
        read,write,put=p.read,p.write,p.put_uint;p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
    obj,profile,player,first,second=base,base+0x1000,base+0x4000,base+0x5000,base+0x6000
    write(base,b'\xa5'*0x7000);put(0x765ad4 if pc else 0x49fc7c,profile);put(profile+0x2b4,player);put(player+0x24,first);put(obj+0x18,second)
    for node,position,axis in [(first,c['first'],c['firstAxis']),(second,c['second'],c['secondAxis'])]:
        write(node+(0x74 if pc else 0x70),struct.pack('<3f',*position));write(node+(0xa4 if pc else 0xa8),struct.pack('<3f',*axis))
    if kind=='helper' or c['slot']==23:a,b=first,second;pos_a,pos_b,axis=c['first'],c['second'],c['secondAxis']
    else:a,b=second,first;pos_a,pos_b,axis=c['second'],c['first'],c['firstAxis']
    threshold=c['threshold'] if kind=='helper' else .25;ignore=c['ignoreY'] if kind=='helper' else 1;negate=c['negate'] if kind=='helper' else 0
    delta=[f32(x-y) for x,y in zip(pos_a,pos_b)]
    if ignore:delta[1]=0.0
    length=math.sqrt(sum(x*x for x in delta));epsilon=struct.unpack('<f',bytes.fromhex('6f12833a'))[0]
    normalized=[f32(x/length) for x in delta] if length>epsilon else [0.0]*3
    dot=sum(x*y for x,y in zip(normalized,axis));dot=-dot if negate else dot;expected=int(dot>threshold)
    before=read(base,0x7000)
    if pc:
        if kind=='helper':p.run(0x597220,args=(a,b,bits(threshold),ignore,negate),callee_pop=False);entry=0x597220
        else:entry={23:0x5906a0,24:0x5906e0}[c['slot']];p.run(entry,this=obj)
        value=p.reg('EAX')&255;assert value==expected,(value,expected,dot,delta)
        result=dict(entry=f'{entry:08X}',returnValue=value,completion='whole original helper' if kind=='helper' else 'whole original GenericTrigger caller and helper',blocks=sum(p.visits.values()),modelDot=dot,modelNormalized=normalized)
    elif kind=='helper':
        p.reg('A0',a);p.reg('A1',b);p.reg('A2',ignore);p.reg('A3',negate);p.reg('F12',bits(threshold))
        result=p.run(entry,[0x2921d8]);observed=[p.reg('F'+str(i))&0xffffffff for i in (2,3,4)]
        assert observed==[bits(v) for v in delta];assert [p.reg('F'+str(i))&0xffffffff for i in (6,7,8)]==[bits(v) for v in axis]
        result.update(completion='original positions/axis/vertical selection before accumulator;normalization/dot not executed',deltaBits=[f'{v:08X}' for v in observed],axisBits=[f'{bits(v):08X}' for v in axis])
    else:
        p.reg('A0',obj);result=p.run(entry,[0x292190]);assert [p.reg('A'+str(i)) for i in range(4)]==[a,b,1,0];assert p.reg('F12')&0xffffffff==bits(.25)
        result.update(completion='original GenericTrigger post-SQ caller at actual helper entry',helperArgs=dict(firstNode='player' if c['slot']==23 else 'trigger',secondNode='trigger' if c['slot']==23 else 'player',ignoreY=1,negate=0,threshold=.25))
    after=read(base,0x7000);assert after==before,'whole borrowed records unchanged'
    result.update(guardedBytes=0x7000,beforeSha256=sha(before),afterSha256=sha(after),scope='PC original normalization/dot;PS2 declared prelude/caller boundary only. Borrowed cached profile and Node records;no normal game binding or arbitrary floating-point equivalence.')
    return result


def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    rows=[c for c in json.loads((FOLDER/'orientation-cases.json').read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='original-trigger-orientation',status='running',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha((FOLDER/'orientation-cases.json').read_bytes()),cases=[]);started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in rows:
        case=dict(input=c);report['cases'].append(case);save()
        try:case.update(status='passed',**execute(c))
        except Exception as e:case.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save();print(json.dumps(dict(status=report['status'],cases=len(rows),failed=failed,seconds=report['seconds'])));return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
