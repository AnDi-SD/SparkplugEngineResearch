#!/usr/bin/env python3
"""Original entity subclass Copy: full PC and explicit PS2 own components.

Distinct finite/raw payloads qualify transfer offsets and untouched bytes.
They are not live entities and are never torn down after pointer-like fields
are modified. PS2 starts after the original parent call, with an explicit
parent result; this does not execute or replace that parent.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime

SELECTION=ROOT/'local-data/results/native-cycle-20260911-0730/entity-payload-copy/scalar-selection-reviewed.json'
PILOT={'wxArrowTrap','wxCharacter','wxIntelliCam'}


def digest(value):return hashlib.sha256(value).hexdigest().upper()


def execute(row,platform,parent_result=1):
    pc=platform=='pc';n=row[platform+'Size'];fields=row[platform+'Fields'];start=0x124 if pc else 0x130
    if pc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
        p=f.p;read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
        p.seams.pop(0x412f70);put(0x74e060,0);f.call(0x52fd90,0x755588);manager=f.call(0x412540);put(0x74e060,manager)
        setup=dict(source='two original factories',originalCloneManager=manager)
        if row['className']=='wxLabelDisplayText':
            f.call(0x6d38e0);engine=p.allocate(0x158);scene=p.allocate(0x54)
            write(engine,bytes(0x158));write(scene,bytes(0x54));put(0x755274,engine)
            root=f.call(0x421e20);put(engine+0x18,scene);put(scene+0x14,root)
            setup['scene']='borrowed engine/scene referencing original empty Node'
        p.execution_profile='protected-block' if row['className']=='wxProjectileManager' else 'micro'
        setup['factoryProfile']=p.execution_profile
        source=f.call(row['pcFactory']);destination=f.call(row['pcFactory'])
        p.execution_profile='micro'
        assert f.allocations[source]==f.allocations[destination]==n
        assert p.uint(source)==p.uint(destination)==int(row['pcVtable'],16)
        assert p.uint(source+0x10)==p.uint(destination+0x10)==0,'empty named parent'
    else:
        p=Ps2ScalarPrefix([(row['ps2Start'],row['ps2WindowSize']),(0x104f00,8)])
        p.map(0x21000000,0x6000);p.map(0x22000000,4096);p.map(0x49f000,4096)
        read,write,put=p.read,p.write,p.put_uint;source,destination,manager=0x21000000,0x21002000,0x21005000
        write(source,b'\xa5'*n);write(destination,b'\x5a'*n)
        put(source,int(row['ps2Vtable'],16));put(destination,int(row['ps2Vtable'],16));put(0x49f810,manager)
        p.reg('GP',0x4a4170);p.reg('SP',0x22000800);p.reg('A0',source);p.reg('A1',destination)
        for reg,role in row['ps2Bindings'].items():p.reg(reg,{'source':source,'destination':destination,'zero':0}[role])
        p.reg('V0',parent_result)
        setup=dict(source='bounded borrowed records',parentResult=parent_result,parent='explicit input after original parent call;parent not executed',cloneManager='opaque receiver for actual no-op hook')
    # Preserve the constructed PC base, except four already qualified wxEntity
    # copy fields. Distinct own payload includes fields this Copy must retain.
    for address,pattern in ((source,0x3f000000),(destination,0x40000000)):
        raw=b''.join(struct.pack('<I',pattern+(off-start)//4*257) for off in range(start,n,4))
        write(address+start,raw[:n-start])
    if pc:
        for off,value in ((0x28,0xa5),(0x38,0x80),(0x39,3)):write(source+off,bytes([value]))
        put(source+0x3c,0x3f400000)
    for i,v in enumerate(fields):
        value=bytes([0xa5]) if v['bytes']==1 else struct.pack('<I',0x3f100000+i*0x101)
        write(source+v['source'],value)
        write(destination+v['destination'],b'\x5a' if v['bytes']==1 else struct.pack('<I',0x40100000+i*0x101))
    before_source=read(source,n);before=read(destination,n);expected=bytearray(before);expected_source=bytearray(before_source)
    reverse=row[platform+'ReverseFields']
    if parent_result:
        for v in reverse:expected_source[v['destination']:v['destination']+v['bytes']]=before[v['source']:v['source']+v['bytes']]
    if pc:
        for off,width in ((0x28,1),(0x38,1),(0x39,1),(0x3c,4)):expected[off:off+width]=before_source[off:off+width]
    if parent_result:
        for v in fields:expected[v['destination']:v['destination']+v['bytes']]=before_source[v['source']:v['source']+v['bytes']]
    if pc:
        value=f.call(int(row['pcEntry'],16),source,(destination,))
        result=dict(entry=row['pcEntry'],completion='whole original Copy return',returnLowByte=value&255,blocks=sum(p.visits.values()))
        assert value&255==1
    else:
        result=p.run(row['ps2Start'],[row['ps2End']],count=3000,timeout_us=500000)
        assert p.reg('V0')==(1 if parent_result else 0)
        result['completion']='own component after parent, before register restore'
    after_source=read(source,n);after=read(destination,n)
    assert after_source==expected_source,'source byte guard including original reverse copies'
    assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:20]
    result.update(setup=setup,bytes=n,sourceBeforeSha256=digest(before_source),sourceAfterSha256=digest(after_source),
        destinationBeforeSha256=digest(before),destinationAfterSha256=digest(after),
        changedOffsets=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b],
        sourceChangedOffsets=[i for i,(a,b) in enumerate(zip(before_source,after_source)) if a!=b],
        reverseTransfers=[dict(**v,destinationInputBytes=before[v['source']:v['source']+v['bytes']].hex(),sourceBeforeBytes=before_source[v['destination']:v['destination']+v['bytes']].hex(),sourceAfterBytes=after_source[v['destination']:v['destination']+v['bytes']].hex()) for v in reverse],
        transfers=[dict(**v,sourceBytes=before_source[v['source']:v['source']+v['bytes']].hex(),destinationBeforeBytes=before[v['destination']:v['destination']+v['bytes']].hex(),destinationAfterBytes=after[v['destination']:v['destination']+v['bytes']].hex()) for v in fields],
        allSourceAndDestinationBytesChecked=True,modifiedPayloadTeardownExecuted=False)
    return result


def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local result required')
    if selection not in ('pilot','batch','cabinet-retry'):raise ValueError('Explicit case selection')
    rows=json.loads(SELECTION.read_text())['classes']
    rows=[r for r in rows if r['className']=='wxCabinetPuzzle'] if selection=='cabinet-retry' else [r for r in rows if (r['className'] in PILOT)==(selection=='pilot')]
    report=dict(kind='original-entity-modified-copy',status='running',selection=selection,inputs=EXPECTED,
        sourceSha256=digest(Path(__file__).read_bytes()),selectionSha256=digest(SELECTION.read_bytes()),cases=[],
        scope='24 entity subclasses. PC original factories then full Copy with changed own fields and known base payload. PS2 own component conditional on parent success/failure. Raw pointer-like values not dereferenced or torn down. No full PS2 Copy/lifetime claim.')
    started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    output.parent.mkdir(parents=True,exist_ok=True)
    for row in rows:
        plan=(('pc',1),('ps2',1)) if selection=='cabinet-retry' else (('pc',1),('ps2',1),('ps2',0))
        for platform,parent_result in plan:
            case=dict(className=row['className'],platform=platform,parentResult=parent_result);report['cases'].append(case);save()
            try:case.update(status='passed',**execute(row,platform,parent_result))
            except Exception as error:case.update(status='blocked',error=str(error),traceback=traceback.format_exc())
            save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
