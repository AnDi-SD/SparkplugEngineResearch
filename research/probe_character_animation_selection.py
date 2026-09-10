#!/usr/bin/env python3
"""Original packed-key animation lookup over explicit borrowed search trees.

PC full selector and search helper; PS2 post-SQ selector and full scalar find.
Missing mandatory default stops before the original sentinel payload read.
No owned insertion,normal animation loading or missing-default value is invented.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime


def execute(platform,c):
    ispc=platform=='pc';profile=c['profile'];key=c['key'];assert 0<=profile<68
    tables={int(k):dict(v) for k,v in c['tables'].items()};assert all(0<=i<68 for i in tables)
    queries=[(profile,key)];selected=tables.get(profile,{}).get(key)
    if key not in tables.get(profile,{}):
        if profile==0:
            queries.append((66,key));selected=tables.get(66,{}).get(key)
        if (profile!=0 or key not in tables.get(66,{})):
            queries.append((profile,0));selected=tables.get(profile,{}).get(0)
    missing=selected is None
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;storage=p.allocate(0x4000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(0x273d10,0x110),(0x274a40,0x78),(0x49c998,4)])
        storage=0x21000000;p.map(storage,0x4000);p.map(0x22000000,0x1000)
        p.reg('SP',0x22000800);p.reg('GP',0x4a4170);read,write,put=p.read,p.write,p.put_uint
    write(storage,b'\xcc'*0x4000);manager=storage;array=manager+(0x20 if ispc else 0x24);stride=12 if ispc else 16
    put(manager,0x703470 if ispc else 0x492ff0);next_node=storage+0x2000;roots={};heads={}
    for index in range(68):
        tree=array+index*stride;head=storage+0x800+index*0x20 if ispc else tree+4;heads[index]=head
        items=sorted(tables.get(index,{}).items())
        def build(items,parent):
            nonlocal next_node
            if not items:return head if ispc else 0
            mid=len(items)//2;a=next_node;next_node+=0x20
            left=build(items[:mid],a);right=build(items[mid+1:],a)
            put(a,left);put(a+(4 if ispc else 8),parent);put(a+(8 if ispc else 4),right)
            put(a+0xc,items[mid][0]);put(a+0x10,items[mid][1])
            if ispc:write(a+0x14,b'\x01\x00')
            return a
        root=build(items,head);roots[index]=root
        if ispc:
            put(tree+4,head);put(tree+8,len(items));put(head+4,root)
            put(head,head);put(head+8,head);write(head+0x14,b'\x01\x01')
        else:put(tree+4,root)
    assert next_node<storage+0x4000
    before=read(storage,0x4000);observed=[]
    def observe(u,a,n,user):
        if a==(0x5e79e0 if ispc else 0x274a40):
            tree=p.reg('ECX') if ispc else p.reg('A1');offset=tree-array;assert offset%stride==0
            keyptr=p.uint(p.reg('ESP')+8) if ispc else p.reg('A2');observed.append((offset//stride,p.uint(keyptr)))
    (p.mu if ispc else p.u).hook_add(p.uc.UC_HOOK_CODE if ispc else __import__('unicorn').UC_HOOK_CODE,observe)
    if ispc:
        p.run(0x59a5e0,manager,(key,profile),stop_at=0x59a64c if missing else None)
        result=dict(entry='0059A5E0',stop=f'{p.reg("EIP"):08X}',completion='sentinel payload read boundary;guest discarded' if missing else 'original return',blocks=sum(p.visits.values()))
        value=p.reg('EAX')&0xffffffff
    else:
        p.reg('S2',manager);p.reg('S1',profile);p.reg('A1',key)
        # Stop on the branch before its load delay slot. The first run stopped
        # on273D78 but Unicorn completed that delay-slot load before yielding.
        result=p.run(0x273d2c,[0x273d74 if missing else 0x273de8],timeout_us=500000)
        result['originalEntry']='00273D10';value=p.reg('V0')&0xffffffff
    assert observed==queries,(observed,queries)
    assert value==(heads[profile] if missing else selected),(hex(value),hex(heads[profile] if missing else selected))
    after=read(storage,0x4000);assert after==before
    result.update(queries=[dict(profile=i,key=k) for i,k in observed],missingMandatoryDefault=missing,
        returnedValue=None if missing else value,readBoundary=dict(node=value,payloadAddress=value+0x10) if missing else None,
        managerBytes=0x350 if ispc else 0x464,borrowedStorageBytes=0x4000,beforeSha256=hashlib.sha256(before).hexdigest().upper(),afterSha256=hashlib.sha256(after).hexdigest().upper())
    return result


def guest(output,selection):
    output=Path(output).resolve();selection=Path(selection).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    cases=json.loads(selection.read_text())['cases'];assert 0<len(cases)<=100
    started=time.perf_counter();report=dict(kind='original-character-animation-table-selection',status='running',inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selectionSha256=hashlib.sha256(selection.read_bytes()).hexdigest().upper(),
        scope='68 indexed roots and explicit borrowed nodes;not an owned or initialized animation database. PC nil-node flags and PS2 null-child layouts follow actual lookup instructions.',cases=[])
    output.parent.mkdir(parents=True,exist_ok=True)
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in cases:
        report['pending']=c;save()
        try:result=dict(input=c,status='passed',**execute(c['platform'],c))
        except Exception as error:result=dict(input=c,status='blocked',error=str(error),traceback=traceback.format_exc())
        report['cases'].append(result);report.pop('pending');save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='passed' if not failed else 'partial';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
