#!/usr/bin/env python3
"""New complete PS2 arithmetic paths compared with saved original PC results."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from functools import lru_cache
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from ps2_scalar_prefix import Ps2ScalarPrefix

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ps2-integer-square-accumulator'
DISTANCE={'wxArrowTrap':(0x382870,0x78,0x1a4),'wxMovingPlatform':(0x390c60,0x98,0x174),'wxPendulum':(0x392030,0x98,0x164)}


def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
def sha(v):return hashlib.sha256(v).hexdigest().upper()
@lru_cache(None)
def read_reference(path,digest):
    raw=(ROOT/path).read_bytes();assert sha(raw)==digest;return json.loads(raw)


def execute(row):
    reference=row['reference'];original=read_reference(reference['path'],reference['sha256'])['cases'][reference['caseIndex']]
    assert original['status']=='passed' and original['input']==row['input'];c=row['input'];kind=row['kind']
    if kind=='distance':
        entry,size,flag=DISTANCE[row['className']];ranges=[(entry,size)]
        assert original['returnLowByte']==row['expectedReturn']
    else:
        entry=0x293210 if kind=='helper' else 0x3a2e78;ranges=[(0x293210,0xd0),(0x423b58,0x28),(0x109af0,0x14)]
        if kind=='gem':ranges.append((0x3a2e78,0x7c))
        assert original['returnValue']==row['expectedReturn']
    p=Ps2ScalarPrefix(ranges,profile='integer-squares');p.map(0x21000000,0x8000);p.map(0x22000000,4096);p.map(0x49f000,4096)
    base=0x21000000;obj,record,other_record,node,other_node,point=base,base+0x1000,base+0x2000,base+0x3000,base+0x4000,base+0x5000
    p.write(base,b'\xa5'*0x8000);p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
    if kind=='distance':
        p.put_uint(obj+0x18,node);p.put_uint(obj+0x80,c['link']);p.put_uint(0x49fde4,c['globalWord']);p.write(obj+flag,bytes([c['flag']]))
        p.write(node+0x70,struct.pack('<3f',*c['node']));p.write(point+0x18,struct.pack('<3f',*c['origin']));p.reg('A0',obj);p.reg('A1',point)
    else:
        p.write(node+0x70,struct.pack('<3f',*c['second']));p.write(other_node+0x70,struct.pack('<3f',*c['first']))
        if kind=='helper':p.reg('A0',other_node);p.reg('A1',node);p.reg('A2',c['force']);p.reg('F12',bits(c['tolerance']))
        else:
            p.put_uint(obj,0x4976a0);p.put_uint(obj+0x24,record);p.put_uint(record+0x24,node);p.put_uint(other_record+0x24,other_node);p.put_uint(obj+0x150,other_record)
            p.put_uint(record+0xc,0xffffff00|(2 if c['enabled'] else 0)|(8 if c['pause'] else 0))
            p.write(obj+0x155,bytes([c['disabled']]));p.write(obj+0x154,bytes([c['override']]));p.put_uint(obj+0x14c,bits(c['radius']));p.reg('S1',obj)
    before=p.read(base,0x8000);wanted=bytearray(before)
    if kind=='distance':wanted[flag]=row['expectedFlag']
    code_before=[p.read(a,n) for a,n in ranges];stop=0x3a2ef4 if kind=='gem' else p.RETURN
    result=p.run(entry,[stop],count=5000,timeout_us=500000)
    value=struct.unpack('<f',struct.pack('<I',p.reg('F0')&0xffffffff))[0] if kind=='helper' else p.reg('V0')
    assert value==row['expectedReturn'],(value,row['expectedReturn'])
    if kind in ('helper','gem'):
        square=struct.unpack('<f',struct.pack('<I',p.reg('F0')&0xffffffff))[0];assert square==row['expectedSquare']
        result['observedSquaredDistance']=square
    after=p.read(base,0x8000);assert after==wanted,'complete borrowed record guard'
    assert [p.read(a,n) for a,n in ranges]==code_before,'original code bytes unchanged'
    assert len(result['accumulatorInterpretation'])==3
    result.update(completion='whole original method through explicit integer-square CPU profile' if kind!='gem' else 'complete own Gem component after v10;helper now returns;before SQ/LQ restore',
        result=value,reference=reference,guardedBytes=0x8000,beforeSha256=sha(before),afterSha256=sha(after),
        changedOffsets=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b],codeRanges=[dict(address=f'{a:08X}',bytes=n,sha256=sha(raw)) for (a,n),raw in zip(ranges,code_before)],
        originalCodeUnchanged=True,context='borrowed records;integer-square CPU interpretation explicit;Gem v10 flags remain declared input,not a full Gem caller/parent execution')
    return result


def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    rows=[x for x in json.loads((FOLDER/'integration-selection.json').read_text()) if x['pilot']==(selection=='pilot')]
    report=dict(kind='ps2-integer-square-integration',status='running',selection=selection,inputs=EXPECTED,
        sourceSha256=sha(Path(__file__).read_bytes()),wrapperSha256=sha((ROOT/'research/ps2_scalar_prefix.py').read_bytes()),extensionSha256=sha((ROOT/'research/ps2_integer_square_accumulator.py').read_bytes()),selectionSha256=sha((FOLDER/'integration-selection.json').read_bytes()),cases=[],
        scope='New original PS2 paths with exact integer-square CPU interpretation compared to saved PC results;PC cases not rerun. Code bytes unchanged. Not general EE FPU or full Gem v10/SQ/LQ.')
    started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for row in rows:
        case=dict(input=row);report['cases'].append(case);save()
        try:case.update(status='passed',**execute(row))
        except Exception as e:case.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(x['status']!='passed' for x in report['cases']);report['status']='partial' if failed else 'passed';save()
    print(json.dumps(dict(status=report['status'],cases=len(rows),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
