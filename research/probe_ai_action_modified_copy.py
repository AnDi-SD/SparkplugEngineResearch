#!/usr/bin/env python3
"""Modified-payload AIAction Copy: full PC calls and qualified PS2 components.

PC objects are constructed through their original factories and an original
empty Node. The source/target payload is then deliberately modified, with
empty cleanup containers retained. These are Copy inputs, not active actors.
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

SELECTION=ROOT/'local-data/results/native-cycle-20260911-0730/ai-action-copy/copy-selection.json'


def fill_payload(write,address,size,start,source):
    raw=bytearray()
    for off in range(start,size,4):raw.extend(struct.pack('<I',(0x3f000000 if source else 0x40000000)+(off//4)*257))
    write(address+start,raw[:size-start])


def execute(row,platform):
    spec=row[platform];ispc=platform=='pc';n=spec['size'];setup=[]
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
        p=f.p;read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
        p.seams.pop(0x412f70);put(0x74e060,0);f.call(0x52fd90,0x755588);manager=f.call(0x412540);put(0x74e060,manager)
        f.call(0x6d38e0);engine=p.allocate(0x158);scene=p.allocate(0x54);write(engine,bytes(0x158));write(scene,bytes(0x54));put(0x755274,engine)
        root=f.call(0x421e20);put(engine+0x18,scene);put(scene+0x14,root)
        source=f.call(row['pcFactory']);destination=f.call(row['pcFactory'])
        assert f.allocations[source]==f.allocations[destination]==n
        assert p.uint(source)==p.uint(destination)==int(spec['vtable'],16)
        assert p.uint(source+0x10)==p.uint(destination+0x10)==p.uint(source+0x1c)==p.uint(destination+0x1c)==0
        setup=dict(source='two original factories',emptySceneRoot=root,originalCloneManager=manager,originalClassExtent=n)
    else:
        ranges=[(0x100320,0x48),(0x104f00,8)]
        ranges += [(0x223268,0x13c),(0x2233c0,0x40)] if spec['base'] else [(spec['entry'],spec['windowSize'])]
        p=Ps2ScalarPrefix(ranges);p.map(0x21000000,0x6000);p.map(0x22000000,4096);p.map(0x49f000,4096)
        read,write,put=p.read,p.write,p.put_uint;source,destination=0x21000000,0x21002000;manager=0x21005000
        write(source,b'\xa5'*n);write(destination,b'\x5a'*n);put(source,int(spec['vtable'],16));put(destination,int(spec['vtable'],16))
        put(source+0x10,0);put(destination+0x10,0);put(source+0x14,0);put(destination+0x14,0)
        put(0x49f810,manager);p.reg('GP',0x4a4170);p.reg('SP',0x22000800);p.reg('A0',source);p.reg('A1',destination)
        if spec['base']:p.reg('S2',source)
        else:
            for reg,role in spec['bindings'].items():p.reg(reg,{'source':source,'destination':destination,'zero':0}[role])
            p.reg('V0',1)
        setup=dict(source='guarded borrowed records',parent='actual empty-container base prefix' if spec['base'] else 'own suffix conditional on returned parent success;parent effects separately qualified',cloneManager='opaque non-null completion receiver;original no-op hook;no constructor claim')
    start=0x20 if ispc else 0x24
    fill_payload(write,source,n,start,True);fill_payload(write,destination,n,start,False)
    for field in spec['fields']:
        if field['bytes']==1:write(source+field['source'],b'\xa5')
    before_source=read(source,n);before_destination=read(destination,n);expected=bytearray(before_destination)
    for v in spec['fields']:expected[v['destination']:v['destination']+v['bytes']]=before_source[v['source']:v['source']+v['bytes']]
    if ispc:
        value=f.call(spec['entry'],source,(destination,));execution=dict(entry=f"{spec['entry']:08X}",completion='whole original Copy return',blocks=sum(p.visits.values()),returnLowByte=value&255)
        assert value&255==1
    else:
        end=0x2233a4 if spec['base'] else spec['end'];execution=p.run(spec['entry'],[end]);assert p.reg('V0')==1
    after_source=read(source,n);after_destination=read(destination,n)
    assert after_source==before_source,'source byte guard'
    assert after_destination==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after_destination,expected)) if a!=b][:15]
    execution.update(setup=setup,bytes=n,fields=spec['fields'],sourceBeforeSha256=hashlib.sha256(before_source).hexdigest().upper(),
        sourceAfterSha256=hashlib.sha256(after_source).hexdigest().upper(),destinationBeforeSha256=hashlib.sha256(before_destination).hexdigest().upper(),
        destinationAfterSha256=hashlib.sha256(after_destination).hexdigest().upper(),
        transfers=[dict(**v,sourceBytes=before_source[v['source']:v['source']+v['bytes']].hex(),destinationBeforeBytes=before_destination[v['destination']:v['destination']+v['bytes']].hex(),destinationAfterBytes=after_destination[v['destination']:v['destination']+v['bytes']].hex()) for v in spec['fields']],
        changedOffsets=[i for i,(a,b) in enumerate(zip(before_destination,after_destination)) if a!=b],modifiedPayloadTeardownExecuted=False)
    return execution


def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter();rows=json.loads(SELECTION.read_text())
    rows=rows[:2] if selection=='pilot' else rows[2:]
    report=dict(kind='paired-original-ai-action-modified-copy',status='running',selection=selection,inputs=EXPECTED,cases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selectionSha256=hashlib.sha256(SELECTION.read_bytes()).hexdigest().upper(),
        scope='This is source,argument is destination. Modified payload with distinct finite word patterns/raw byteA5;empty destination cleanup. PC full Copy;PS2 base prefix or conditional own suffix;no modified-payload lifetime/active gameplay claim.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for row in rows:
        case=dict(className=row['className'],platforms={});report['cases'].append(case)
        for platform in ('pc','ps2'):
            report['pending']=dict(className=row['className'],platform=platform);save()
            try:case['platforms'][platform]=dict(status='passed',**execute(row,platform))
            except Exception as error:case['platforms'][platform]=dict(status='blocked',error=str(error),traceback=traceback.format_exc())
            report.pop('pending');save()
    failures=sum(v['status']!='passed' for c in report['cases'] for v in c['platforms'].values())
    report['status']='passed' if not failures else 'partial';save();print(json.dumps(dict(status=report['status'],classes=len(rows),platformCases=len(rows)*2,failed=failures,seconds=report['seconds'])))
    return int(failures!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
