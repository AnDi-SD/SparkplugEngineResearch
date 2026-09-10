#!/usr/bin/env python3
"""Original post-classification entity insertion branches on PC and PS2.

Both platforms start after the separately verified classifier. Button cases
start after diagnostics. Stop before the real linked-list append or epilogue;
neither omitted game work nor diagnostics receives a substituted success.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix
from capture_native_ranges import EXPECTED
import probe_pc_animation_lifecycle as lifetime

LAYOUT={'pc':dict(size=0x5df8,capacity=130,moveArray=0x55f8,moveCount=0x5918,hudArray=0x591c,hudCount=0x5aac,buttonArray=0x5ab0,buttonCount=0x5cb8,buttonCapacity=130),
        'ps2':dict(size=0x3e4c,capacity=80,moveArray=0x36b8,moveCount=0x39d8,hudArray=0x39dc,hudCount=0x3b6c,buttonArray=0x3b70,buttonCount=0x3d14,buttonCapacity=105)}


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    started=time.perf_counter();report=dict(kind='paired-original-entity-manager-insertion-prefixes',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),layouts=LAYOUT,
        scope=__doc__,limits='Fresh guests.PC block100k/2s;PS2 ordinary2000/100ms;30s outer,default64KiB PC arena. Whole manager byte guards.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    cases=[dict(kind='existing',count=c) for c in ('zero','last','full','above')]
    cases += [dict(kind='existing',count='one',duplicate=d) for d in (0,4,'last')]
    cases += [dict(kind='existing',count='zero',duplicate='last')]
    cases += [dict(kind='new',freeIndex=i) for i in (1,39,-1)]
    cases += [dict(kind='hud',count=c) for c in ('zero','last','full')]
    cases += [dict(kind='button',count=c) for c in ('last','full')]
    cases += [dict(kind='move',count='zero',secondary=c) for c in ('zero','full')]
    def prepare(case,platform,entity):
        l=LAYOUT[platform];data=bytearray(b'\xa5'*l['size'])
        def put(off,v):data[off:off+4]=struct.pack('<I',v&0xffffffff)
        def zero(off,n):data[off:off+n*4]=bytes(n*4)
        zero(0x54,40);zero(0x194,40*l['capacity']);put(0xf4,0)
        for i in range(1,40):put(0xf4+4*i,0xffffffff)
        for arr,count,capacity in ((l['moveArray'],l['moveCount'],l['capacity']),(l['hudArray'],l['hudCount'],100),(l['buttonArray'],l['buttonCount'],l['buttonCapacity'])):zero(arr,capacity);put(count,0)
        kind=case['kind'];key=0x680838e4 if kind=='move' else 0x7f2e3f1a if kind=='hud' else 0x69f878ba if kind=='button' else 0x12345678
        group=41 if kind=='hud' else 42 if kind=='button' else -1 if kind=='new' else 3
        capacity=100 if kind=='hud' else l['buttonCapacity'] if kind=='button' else l['capacity']
        count={'zero':0,'one':1,'last':capacity-1,'full':capacity,'above':capacity+1}.get(case.get('count'),0)
        arr=l['hudArray'] if kind=='hud' else l['buttonArray'] if kind=='button' else 0x194+max(group,0)*l['capacity']*4
        counter=l['hudCount'] if kind=='hud' else l['buttonCount'] if kind=='button' else 0x54+max(group,0)*4
        if kind!='new':
            put(counter,count)
            if kind in ('existing','move'):put(0xf4+group*4,key)
            for i in range(min(count,capacity)):put(arr+4*i,0x25000000+i*4)
        duplicate=case.get('duplicate');duplicate=capacity-1 if duplicate=='last' else duplicate
        if duplicate is not None:put(arr+4*duplicate,entity)
        secondary=l['capacity'] if case.get('secondary')=='full' else 0
        if kind=='move':put(l['moveCount'],secondary)
        before=bytes(data);accepted=False
        if kind=='new':
            free=case['freeIndex']
            if free>=0:
                accepted=True;put(0x194+free*l['capacity']*4,entity);put(0xf4+free*4,key);put(0x54+free*4,1)
        elif count<capacity and duplicate is None:
            accepted=True;put(arr+4*count,entity);put(counter,count+1)
        if accepted and kind=='move' and secondary<l['capacity']:
            put(l['moveArray']+secondary*4,entity);put(l['moveCount'],secondary+1)
        return before,bytes(data),dict(group=group,key=key,firstFree=case.get('freeIndex',-1),accepted=accepted,capacity=capacity,initialCount=count,array=arr,counter=counter,secondary=secondary)
    try:
        for case in cases:
            report['pending']=case;save();results={}
            for platform in ('pc','ps2'):
                l=LAYOUT[platform]
                if platform=='pc':
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;manager=p.allocate(l['size']);entity=p.allocate(0x28)
                    before,expected,contract=prepare(case,platform,entity);p.mu.mem_write(manager,before)
                    entry=0x574982 if case['kind']=='button' else 0x574933;stop=0x574a9b if contract['accepted'] else 0x574ac5
                    prepared=[]
                    def initialize(u,a,n,user):
                        assert not prepared;prepared.append(a)
                        for name,value in (('EAX',contract['group']&0xffffffff),('ESI',manager),('EDI',entity),('EBX',contract['key'])):p.set_reg(name,value)
                        p.put_uint(p.reg('ESP')+0x10,contract['firstFree']);p.put_uint(p.reg('ESP')+0x14,contract['key'])
                    p.mu.hook_add(p.uc.UC_HOOK_CODE,initialize,begin=entry,end=entry)
                    p.run(entry,stop_at=stop);assert prepared and bytes(p.mu.mem_read(manager,l['size']))==expected,(case,platform)
                    results[platform]=dict(entry=f'{entry:08X}',stop=f'{stop:08X}',blocks=sum(p.visits.values()),contract=contract,wholeManagerGuard=True)
                else:
                    entry=0x28952c if case['kind']=='button' else 0x2893cc
                    p=Ps2ScalarPrefix([(0x2893cc,0x20c)]);manager=0x21000000;entity=0x21005000;stack=0x22000800
                    p.map(manager,0x6000);p.map(0x22000000,4096);before,expected,contract=prepare(case,platform,entity);p.write(manager,before)
                    p.put_uint(stack+0x40,entity);p.put_uint(stack+0x4c,contract['firstFree'])
                    group=contract['group'] if contract['group']>=0 else 0xffffffffffffffff
                    key=contract['key'] if contract['key']<0x80000000 else contract['key']|0xffffffff00000000
                    for name,value in (('V0',group),('S2',manager),('S0',key),('S1',0),('SP',stack)):p.reg(name,value)
                    stop=0x289498 if contract['accepted'] else 0x2895d8
                    result=p.run(entry,[stop]);assert p.read(manager,l['size'])==expected,(case,platform)
                    result.update(contract=contract,wholeManagerGuard=True);results[platform]=result
            assert results['pc']['contract']['accepted']==results['ps2']['contract']['accepted']
            report['cases'].append(dict(input=case,**results));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
