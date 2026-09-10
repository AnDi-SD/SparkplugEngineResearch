#!/usr/bin/env python3
"""Original character/animation gates and entity distance flags.

PS2 accumulator opcodes are not emulated. Distance prelude and comparison
tail run in separate fresh guests with explicit finite comparison inputs.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/entity-runtime-methods'
SPECS={
 'wxCharacter':dict(pc=0x4f4070,ps2=0x2a9670,size=0x40),
 'wxAnimationController':dict(pc=0x4fb510,ps2=0x2a6fa0,size=0xd0),
 'wxArrowTrap':dict(pc=0x581080,ps2=0x382870,size=0x78,flag=0x198,prefixEnd=0x3828b0,tail=0x3828c0),
 'wxMovingPlatform':dict(pc=0x57e7e0,ps2=0x390c60,size=0x98,flag=0x168,prefixEnd=0x390cc0,tail=0x390cd0),
 'wxPendulum':dict(pc=0x5806e0,ps2=0x392030,size=0x98,flag=0x158,prefixEnd=0x392090,tail=0x3920a0)}


def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
def sha(v):return hashlib.sha256(v).hexdigest().upper()


def execute(c):
    name=c['className'];spec=SPECS[name];pc=c['platform']=='pc';d=0 if pc else 12;phase=c['phase']
    if pc:
        p=PcBlocks();base=p.allocate(0x6000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(spec['ps2'],spec['size'])]);base=0x21000000;p.map(base,0x6000);p.map(0x22000000,4096);p.map(0x49f000,4096)
        read,write,put=p.read,p.write,p.put_uint;p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
    obj=base;record=base+0x1000;character=base+0x2000;actor=base+0x3000;node=base+0x4000;point=base+0x5000
    write(base,b'\xa5'*0x6000);put(obj+0x24,record)
    wanted=bytearray(read(base,0x6000));expected_value=0;extra={}
    if c['kind']=='character':
        put(obj+0x148+d,c['link']);put(record+0xc,c['flags']);expected_value=int(not(c['flags']&(0x1a if c['link'] else 8)))
    elif c['kind']=='animation':
        put(record+0xc,c['flags']);put(obj+0x124+d,0 if c['character']=='absent' else character)
        put(character+0x148+d,1 if c['character']=='present-set' else 0);put(obj+0x128+d,actor if c['actor'] else 0)
        put(obj+0x17c+d,c['handle']);write(obj+0x184+d,bytes([c['force']]));write(actor+0x1c,bytes([c['active']]))
    else:
        put(obj+0x18,node);put(obj+0x80,c['link']);put(0x765ab8 if pc else 0x49fde4,c['globalWord'])
        write(node+(0x74 if pc else 0x70),struct.pack('<3f',*c['node']));write(point+0x18,struct.pack('<3f',*c['origin']))
        write(obj+spec['flag']+d,bytes([c['flag']]));delta=[a-b for a,b in zip(c['node'],c['origin'])];square=sum(v*v for v in delta)
        skip=name!='wxArrowTrap' and c['globalWord']!=0 and c['link']!=0
        assert (phase=='full')==(pc or bool(skip))
        expected_value=1;extra=dict(delta=delta,squaredDistanceInput=square,thresholdBits='49F42400',threshold=2000000,skip=skip)
        if not pc and phase=='tail':
            # A fresh independent comparison component; no resumed prefix and
            # no invented accumulator implementation. Initial flag1 comes from
            # the separately observed original prelude, comparison input explicit.
            write(obj+spec['flag']+d,b'\1');p.reg('SP',0x220007f0)
            for reg,value in [('F0',bits(2000000)),('F1',bits(square)),('F3',bits(delta[1])),('F5',bits(delta[2]))]:p.reg(reg,value)
    before=read(base,0x6000);wanted=bytearray(before)
    if c['kind']=='animation':
        mask=8 if c['character']=='present-clear' else 0x1a;enable=bool(c['force']) or not(c['flags']&mask)
        if c['actor']:
            if enable and c['handle'] and not c['active']:wanted[0x301c]=1;wanted[0x3024]=1
            elif not enable and c['active']:wanted[0x301c]=0;wanted[0x3024]=1
        wanted[0x184+d]=0;extra=dict(mask=mask,enable=enable)
    elif c['kind']=='distance' and not extra['skip']:
        wanted[spec['flag']+d]=1 if phase=='prefix' else int(extra['squaredDistanceInput']<=2000000)
    if pc:
        p.run(spec['pc'],this=obj,args=(point,));value=p.reg('EAX')&255;result=dict(entry=f'{spec["pc"]:08X}',completion='whole original method return',blocks=sum(p.visits.values()),returnLowByte=value)
        assert value==expected_value
    else:
        p.reg('A0',obj);p.reg('A1',point)
        entry=spec['tail'] if phase=='tail' else spec['ps2'];stop=spec['prefixEnd'] if phase=='prefix' else p.RETURN
        result=p.run(entry,[stop],count=4000,timeout_us=500000)
        result['completion']='original prelude before R5900 accumulator' if phase=='prefix' else 'independent comparison tail with explicit finite square' if phase=='tail' else 'whole original method return'
        if phase=='prefix':
            actual=[p.reg(reg)&0xffffffff for reg in ['F1','F3','F5']];assert actual==[bits(x) for x in extra['delta']];result['deltaBits']=[f'{v:08X}' for v in actual]
            assert p.reg('F0')&0xffffffff==bits(2000000)
        else:assert p.reg('V0')==expected_value;result['returnValue']=p.reg('V0')
    after=read(base,0x6000);assert after==wanted,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,wanted)) if a!=b][:20]
    result.update(**extra,context='bounded borrowed object/record/character/actor/node/point storage;no constructor or normal game binding claim',beforeSha256=sha(before),afterSha256=sha(after),
        changedBytes=[dict(offset=i,before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b],guardedBytes=0x6000)
    return result


def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result')
    cases=json.loads((FOLDER/'predicate-cases.json').read_text())
    if selection=='pilot':cases=[c for c in cases if c['pilot']]
    elif selection.startswith('batch-'):
        index=int(selection[6:]);assert 0<=index<=4;cases=[c for c in cases if not c['pilot']][index*80:(index+1)*80]
    else:raise ValueError('Explicit selection')
    assert cases
    report=dict(kind='original-entity-runtime-predicates',status='running',selection=selection,inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha((FOLDER/'predicate-cases.json').read_bytes()),cases=[],
        scope='PC all complete. PS2 Character/Animation gates and distance skip complete;distance prelude and tail independent. R5900 squared-distance accumulation remains unexecuted. Normal entity/actor binding not established.')
    started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in cases:
        row=dict(input=c);report['cases'].append(row);save()
        try:row.update(status='passed',**execute(c))
        except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
