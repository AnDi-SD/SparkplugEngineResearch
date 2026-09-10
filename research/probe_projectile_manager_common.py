#!/usr/bin/env python3
"""Original paired projectile manager copy, dispatch and bounded pool paths."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from capture_native_ranges import EXPECTED,read_window
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='paired-original-projectile-manager-common',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='PC original complete no-consumer paths and copy,otherwise stop at actual method/allocation/Node consumer. PS2 fresh prefixes after declared saved-register prologues,complete timer query;own copy after known inherited success. No substitute game callback or full pool lifecycle.',
        limits='PC micro100k/2s;PS2 R4000 scalar2000/100ms;outer30s;whole manager/record/message/dependency guards')
    cases=[dict(kind='notify',code=c) for c in (0,0x1b,0x1c,0x1d,0x1e,0x273e,0x273f,0x2740)]
    cases += [dict(kind='copy',pattern=v) for v in (0,1,2)]
    cases += [dict(kind='register',group=g,occupied=v) for g in (0,3) for v in ((0,1,1,1,1),(1,0,1,1,1),(1,1,1,0,1),(1,1,1,1,0),(1,1,1,1,1))]
    tickdefaults=dict(kind='tick',paused=0,enabled=1,entry=False,active=1,now=100,deadline=100,group=0,slot=0)
    cases += [dict(tickdefaults,**v) for v in (dict(paused=1),dict(paused=255),dict(enabled=0),dict(enabled=255),dict(entry=True,active=0),dict(entry=True,deadline=99),dict(entry=True,deadline=100),dict(entry=True,deadline=101),dict(entry=True,now=0,deadline=0xffffffff),dict(entry=True,now=0xffffffff,deadline=0),dict(entry=True,now=0x80000000,deadline=0x7fffffff),dict(entry=True,group=3,slot=4),dict(entry=True,group=3,slot=4,deadline=99),dict(entry=True,active=255,group=2,slot=3))]
    copy_pc=(0x128,0x124,0x134,0x138,0x13c,0x140,0x144,0x148,0x150,0x154,0x158,0x160,0x164,0x168)
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for case in cases:
            report['pending']=case;save();pair={};kind=case['kind']
            for platform in ('pc','ps2'):
                pc=platform=='pc';size=0x1d0 if pc else 0x1e0;vt=0x6f7008 if pc else 0x49afd0;pool=0x170 if pc else 0x17c
                if pc:
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;obj=p.allocate(size);target=p.allocate(size);msg=p.allocate(32);timer=p.allocate(0x44);record=p.allocate(12);payload=p.allocate(0x30)
                    node=f.call(0x421e20);node_enable=p.uint(p.uint(node)+0x34)
                    read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
                else:
                    p=Ps2ScalarPrefix([(0x2c6590,0x74),(0x2c62b4,0xa4),(0x2c6390,0x13c),(0x28f020,8),(0x2c514c,0xe4)])
                    p.map(0x21000000,0x4000);p.map(0x22000000,4096);p.map(0x49f000,4096);p.map(vt,80);p.map(0x4902f0,0x40)
                    raw,sections=pristine();p.write(vt,read_window('ps2',raw,vt,80,sections)[0]);p.write(0x4902f0,read_window('ps2',raw,0x4902f0,0x40,sections)[0])
                    obj,target,msg,timer,record,payload,node=0x21000000,0x21001000,0x21002000,0x21002100,0x21002200,0x21002300,0x21002400;node_enable=0x1a5b00
                    read,write,put=p.read,p.write,p.put_uint;write(node,b'\x69'*0xb8);put(node,0x4902f0)
                    p.reg('SP',0x22000800);p.reg('GP',0x4a4170);p.reg('A0',obj)
                write(obj,b'\xa5'*size);put(obj,vt);put(obj+0x10,0);write(target,b'\x5a'*size);put(target,vt);put(target+0x10,0)
                write(msg,b'\x96'*32);write(timer,b'\x96'*0x44);write(record,b'\x69'*12);write(payload,b'\x96'*0x30);put(payload+0x24,node)
                stop=None;entry=None;args=();outcome=None;meaning=None;expected_words=None
                if kind=='notify':
                    put(msg,case['code']);entry=0x5061e0 if pc else 0x2c6590;args=(msg,)
                    stop=({0x1c:0x505ea0,0x1e:0x5068a0,0x273f:0x506100} if pc else {0x1c:0x2c64f0,0x1e:0x2c6370,0x273f:0x2c5120}).get(case['code'])
                    meaning={0x1c:'virtual16',0x1e:'virtual17',0x273f:'pool-register'}.get(case['code'],'return')
                    if not pc:p.reg('A1',msg)
                elif kind=='copy':
                    offsets=copy_pc if pc else tuple(o+12 for o in copy_pc)
                    expected_words=[0 if case['pattern']==0 else (i+1)*0x1111111 if case['pattern']==1 else (0x7fc12345,0x80000000,0xffffffff,0x87654321)[i%4] for i in range(len(offsets))]
                    for o,v in zip(offsets,expected_words):put(obj+o,v)
                    entry=0x505f10 if pc else 0x2c62b4;args=(target,);meaning='own-copy';stop=None if pc else 0x104f00
                    if not pc:put(0x49f810,0x21003000);p.reg('S1',obj);p.reg('S0',target)
                else:
                    for i in range(20):put(obj+pool+i*4,0)
                    if kind=='register':
                        put(msg+0x18,case['group']);put(msg+0x1c,payload)
                        for i,v in enumerate(case['occupied']):put(obj+pool+(case['group']*5+i)*4,record if v else 0)
                        first=next((i for i,v in enumerate(case['occupied'][:4]) if not v),None);meaning='allocate' if first is not None else 'full-four';outcome=first
                        entry=0x506100 if pc else 0x2c514c;args=(msg,);stop=(0x4123d0 if pc else 0x10d850) if first is not None else None
                        if not pc:p.reg('S5',obj);p.reg('S4',msg);p.reg('S3',0);p.reg('A1',msg)
                    else:
                        write(timer+0x40,bytes([case['paused']]));put(timer+0x1c,case['now']);put(0x755298 if pc else 0x49fc80,timer)
                        write(obj+(0x1c0 if pc else 0x1cc),bytes([case['enabled']]));put(record,case['deadline']);write(record+4,bytes([case['active']]));put(record+8,payload)
                        if case['entry']:put(obj+pool+(case['group']*5+case['slot'])*4,record)
                        active=not case['paused'] and case['enabled'] and case['entry'] and case['active'];expired=bool(active and case['deadline']<case['now'])
                        meaning='expired' if expired else 'update' if active else 'paused' if case['paused'] else 'return';outcome=not bool(case['paused'])
                        entry=0x5068a0 if pc else 0x2c6390;stop=node_enable if expired else (0x5063d0 if pc else 0x2c57c0) if active else None
                before=read(obj,size);target_before=read(target,size);msg_before=read(msg,32);timer_before=read(timer,0x44);record_before=read(record,12);payload_before=read(payload,0x30);node_before=read(node,0xb8)
                expected=bytearray(before);expected_target=bytearray(target_before);expected_record=bytearray(record_before)
                if kind=='copy':
                    for o in offsets:expected_target[o:o+4]=before[o:o+4]
                    if pc:
                        for o in (0x28,0x38,0x39):expected_target[o]=before[o]
                        expected_target[0x3c:0x40]=before[0x3c:0x40]
                if kind=='tick' and meaning=='expired':expected_record[:4]=b'\0'*4;expected_record[4]=0
                if pc:
                    if stop:p.run(entry,this=obj,args=args,stop_at=stop)
                    else:f.call(entry,obj,args)
                    r=dict(blocks=sum(p.visits.values()),completion='actual original consumer entry' if stop else 'original return')
                    if kind=='copy':assert p.reg('EAX')&255==1
                    if kind=='tick' and not stop:assert (p.reg('EAX')&255)==outcome
                    if kind=='tick' and meaning=='update':assert p.reg('ECX')==obj and (p.uint(p.reg('ESP')+4),p.uint(p.reg('ESP')+8))==(case['group'],case['slot'])
                    if kind=='tick' and meaning=='expired':assert p.reg('ECX')==node and (p.uint(p.reg('ESP')+4),p.uint(p.reg('ESP')+8))==(0,1)
                    if kind=='register' and stop:assert p.uint(p.reg('ESP')+4)==12 and p.reg('EBX')==outcome
                    if kind=='notify' and stop:assert p.reg('ECX')==obj
                else:
                    terminal=stop or (p.RETURN if kind=='notify' else 0x2c522c if kind=='register' else 0x2c64cc)
                    r=p.run(entry,[terminal])
                    if kind=='tick' and not stop:assert p.reg('V0')==outcome
                    if kind=='tick' and meaning=='update':assert p.reg('A0')==obj and (p.reg('A1'),p.reg('A2'))==(case['group'],case['slot'])
                    if kind=='tick' and meaning=='expired':assert (p.reg('A0'),p.reg('A1'),p.reg('A2'))==(node,0,1)
                    if kind=='register' and stop:assert p.reg('A0')==12 and p.reg('S3')==outcome
                    if kind=='notify' and stop:assert p.reg('A0')==obj
                    if kind=='copy':assert p.reg('A0')==0x21003000 and p.reg('A1')==0x7db63b02
                assert read(obj,size)==bytes(expected) and read(target,size)==bytes(expected_target),'whole manager guard'
                assert read(msg,32)==msg_before and read(timer,0x44)==timer_before and read(record,12)==bytes(expected_record) and read(payload,0x30)==payload_before and read(node,0xb8)==node_before,'whole dependency guard'
                r.update(meaning=meaning,outcome=outcome,copiedWords=expected_words);pair[platform]=r
            assert all(pair['pc'][k]==pair['ps2'][k] for k in ('meaning','outcome','copiedWords'))
            report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
