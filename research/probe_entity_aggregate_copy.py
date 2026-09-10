#!/usr/bin/env python3
"""Original entity string/vector/block copies, with explicit PS2 boundaries."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix,unicorn
import probe_pc_animation_lifecycle as lifetime

SELECTION=ROOT/'local-data/results/native-cycle-20260911-0730/entity-aggregate-copy/selection.json'
PILOT={('wxBush','short'),('wxMovingPlatform','finite'),('wxChallengeParams','pattern'),('wxTargetRegion','short')}


def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(row,k,variant):
    pc=k=='pc';success=variant!='parent-false';n=row[k+'Size'];start=0x124 if pc else 0x130;fields=row[k+'Fields']
    boundary=None if pc or not success else row['ps2SuccessBoundary'];calls=[]
    if pc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
        p=f.p;read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
        p.seams.pop(0x412f70);put(0x74e060,0);f.call(0x52fd90,0x755588);manager=f.call(0x412540);put(0x74e060,manager)
        source=f.call(row['pcFactory']);dest=f.call(row['pcFactory']);assert f.allocations[source]==f.allocations[dest]==n
        assert p.uint(source)==p.uint(dest)==int(row['pcVtable'],16) and p.uint(source+0x10)==p.uint(dest+0x10)==0
        context='two original factories;empty names;actual wxEntity parent and CloneManager'
    else:
        ranges=[(row['ps2Start'],row['ps2WindowSize']),(0x104f00,8)]
        if row['strings']:ranges.append((0x40a9f0,0x114))
        if row.get('bulk'):ranges.append((0x4074f0,0xb0))
        p=Ps2ScalarPrefix(ranges);p.map(0x21000000,0x6000);p.map(0x22000000,4096);p.map(0x49f000,4096)
        read,write,put=p.read,p.write,p.put_uint;source,dest,manager=0x21000000,0x21002000,0x21005000
        write(source,b'\xa5'*n);write(dest,b'\x5a'*n);put(source,int(row['ps2Vtable'],16));put(dest,int(row['ps2Vtable'],16));put(0x49f810,manager)
        for reg,value in [('S1',source),('S0',dest),('V0',int(success)),('A0',source),('A1',dest),('GP',0x4a4170),('SP',0x22000800)]:p.reg(reg,value)
        context='16-byte-aligned borrowed records;own component after parent call with explicit V0;parent/SQ/LQ not executed'
        def observe(u,a,n,unused):
            if a==0x40a9f0:calls.append(dict(entry=f'{a:08X}',sourceOffset=p.reg('A1')-source,destinationOffset=p.reg('A0')-dest))
        p.u.hook_add(unicorn.UC_HOOK_CODE,observe)
    for address,pattern in [(source,0x3f000000),(dest,0x40000000)]:
        raw=b''.join(struct.pack('<I',pattern+(off-start)//4*257) for off in range(start,n,4));write(address+start,raw[:n-start])
    for i,v in enumerate(fields):
        b=bytes([0xff if variant=='negative' else 0xa5]) if v['bytes']==1 else struct.pack('<f',(-1 if variant=='negative' else 1)*(i+1)*0.25)
        write(source+v['source'],b);write(dest+v['destination'],b'\x5a' if v['bytes']==1 else struct.pack('<I',0x40500000+i*257))
    strings=[]
    for index,s in enumerate(row['strings']):
        off=s[k+'Offset'];room=s['testRoomBytes']
        if variant=='empty':value=b'\0source-tail'
        elif variant=='near-next-field':value=bytes((65+i%26 for i in range(room-1)))+b'\0'
        elif variant=='embedded-zero':value=b'\xff\x80A\0ignored-tail'
        else:value=b'Test123\0source-tail'
        write(source+off,value);write(dest+off,b'D'*room)
        strings.append(dict(offset=off,bytes=value[:value.index(0)+1],room=room))
    if row.get('bulk'):
        off=row['bulk'][k+'Offset'];count=row['bulk']['bytes']
        value=bytes(range(count)) if variant=='pattern' else bytes((255-i for i in range(count)));write(source+off,value);write(dest+off,b'\x5a'*count)
    before_source=read(source,n);before=read(dest,n);expected=bytearray(before)
    if pc:
        for off,width in [(0x28,1),(0x38,1),(0x39,1),(0x3c,4)]:expected[off:off+width]=before_source[off:off+width]
    if success and not boundary:
        for v in fields:expected[v['destination']:v['destination']+v['bytes']]=before_source[v['source']:v['source']+v['bytes']]
        for s in strings:expected[s['offset']:s['offset']+len(s['bytes'])]=s['bytes']
        if row.get('bulk'):
            off=row['bulk'][k+'Offset'];count=row['bulk']['bytes'];expected[off:off+count]=before_source[off:off+count]
    if pc:
        value=f.call(int(row['pcEntry'],16),source,(dest,));assert value&255==1
        result=dict(entry=row['pcEntry'],completion='whole original Copy return',blocks=sum(p.visits.values()),returnLowByte=value&255)
    else:
        stop=boundary or row['ps2End'];result=p.run(row['ps2Start'],[stop],count=10000,timeout_us=500000)
        if boundary:
            off=row['bulk']['ps2Offset'] if row.get('bulk') else row['strings'][0]['ps2Offset']
            assert p.reg('A0')==dest+off and p.reg('A1')==source+off
            if row.get('bulk'):assert p.reg('A2')==row['bulk']['bytes']
            result.update(completion='real library call entry;aligned R5900 copy not executed',boundaryArgs=dict(sourceOffset=off,destinationOffset=off,bytes=p.reg('A2') if row.get('bulk') else None))
        else:
            assert p.reg('V0')==int(success);result['completion']='own component before register restore'
            expected_calls=[dict(entry='0040A9F0',sourceOffset=s['ps2Offset'],destinationOffset=s['ps2Offset']) for s in row['strings']] if success else []
            assert calls==expected_calls,(calls,expected_calls)
    after_source=read(source,n);after=read(dest,n)
    assert before_source==after_source,'whole source guard'
    assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:20]
    result.update(context=context,bytes=n,sourceBeforeSha256=sha(before_source),sourceAfterSha256=sha(after_source),destinationBeforeSha256=sha(before),destinationAfterSha256=sha(after),
        strings=[dict(offset=s['offset'],copiedInputHex=s['bytes'].hex(),testRoomBytes=s['room'],destinationAfterHex=after[s['offset']:s['offset']+s['room']].hex()) for s in strings],
        fields=fields,libraryCalls=calls,changedOffsets=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b],allSourceDestinationBytesChecked=True,modifiedPayloadTeardownExecuted=False)
    return result


def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local output required')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    rows=json.loads(SELECTION.read_text());cases=[]
    for row in rows:
        variants=('empty','short','near-next-field','embedded-zero') if row['strings'] else ('pattern','inverse') if row.get('bulk') else ('finite','negative')
        for variant in variants:
            if ((row['className'],variant) in PILOT)==(selection=='pilot'):
                for k in ('pc','ps2'):cases.append((row,k,variant))
        if selection=='batch':cases.append((row,'ps2','parent-false'))
    report=dict(kind='original-entity-aggregate-copy',status='running',selection=selection,inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(SELECTION.read_bytes()),cases=[],
        scope='PC full methods;PS2 successful own scalar components or explicit aligned library boundary,plus false-parent no-effects. Actual byte/64bit strcpy only. No MMI/SQ/LQ emulation,modified lifetime or semantic string capacity claim.')
    started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for row,k,variant in cases:
        c=dict(input=dict(className=row['className'],platform=k,variant=variant));report['cases'].append(c);save()
        try:c.update(status='passed',**execute(row,k,variant))
        except Exception as e:c.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
