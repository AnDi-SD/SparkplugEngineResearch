#!/usr/bin/env python3
"""wxEntityManager table defaults and paired type-group classification."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix
from capture_native_ranges import EXPECTED
import probe_pc_animation_lifecycle as lifetime


def guest(output,selection='all'):
    if selection not in ('all','remaining-after17'):raise ValueError('Explicit case selection required')
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    started=time.perf_counter();folder=ROOT/'local-data/results/native-cycle-20260910-1900/entity-manager'
    report=dict(kind='paired-wx-entity-manager-tables-and-classifier',status='running',inputs=EXPECTED,cases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        selection=selection,scope='PC constructor bytes reused from complete original lifetime. PS2 two separate scalar constructor prefixes exclude hardware store and full frame;classifier uses literal manager/entity/player-profile records. No normal scene binding,PS2 hardware execution or game callback replacement.',
        limits='Fresh PC block100k/2s and PS2 ordinary2000/100ms per classifier;constructor loop explicitly20000/100ms;30s outer/default PC64KiB.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def words(data,off,count):return list(struct.unpack('<'+'I'*count,data[off:off+count*4]))
    try:
        source=folder/'pc-wxEntityManager-run1.json';d=json.loads(source.read_text());assert d['status']=='passed'
        b=bytes.fromhex(d['initial']['bytes']);assert len(b)==0x5df8
        assert words(b,0x54,40)==[0]*40 and words(b,0xf4,40)==[0]+[0xffffffff]*39
        assert words(b,0x194,40*130)==[0]*(40*130) and words(b,0x52d4,200)==[0]*200
        assert words(b,0x55f8,130)==[0]*130 and words(b,0x5800,70)==[0xcccccccc]*70
        assert words(b,0x591c,100)==[0]*100
        report['pcDefaultTables']=dict(source=source.relative_to(ROOT).as_posix(),sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest().upper(),
            allocationBytes=0x5df8,typeGroups=40,slotsPerGroup=130,groupCountsOffset=0x54,groupHashesOffset=0xf4,groupPointersOffset=0x194,
            followingZeroRuns=[dict(offset=0x52d4,words=200),dict(offset=0x55f8,words=130),dict(offset=0x591c,words=100)],
            untouchedWordsAfterThirdRun=dict(offset=0x5800,words=70,fill='CCCCCCCC'))
        save()
        obj=0x21000000
        for kind,entry,end in ((('before-hardware',0x2897f8,0x289820),('table-loops',0x289824,0x2898f8)) if selection=='all' else ()):
            p=Ps2ScalarPrefix([(entry,end-entry)]);p.map(obj,0x4000);p.write(obj,b'\xcc'*0x3e4c);p.reg('S0',obj)
            p.reg('A2',obj);p.reg('T0',0);before=p.read(obj,0x3e4c);expected=bytearray(before)
            r=p.run(entry,[end],count=20000 if kind=='table-loops' else 2000)
            def put(off,value):expected[off:off+4]=struct.pack('<I',value)
            if kind=='before-hardware':
                for off in (0x50,0x39d8,0x3b6c,0x3d14):put(off,0)
                expected[0x36b4]=1
                assert p.reg('V0')==0x10000000 and p.reg('V1')==0x82
                r['excludedHardwareStore']=dict(address='10000010',value='00000082',executed=False)
            else:
                put(0x3e30,0)
                for i in range(40):put(0x54+4*i,0);put(0xf4+4*i,0 if i==0 else 0xffffffff)
                for off,count in ((0x194,40*80),(0x3394,200),(0x36b8,80)):
                    for i in range(count):put(off+4*i,0)
                assert p.read(obj+0x37f8,120*4)==b'\xcc'*(120*4)
                r['tableShape']=dict(typeGroups=40,slotsPerGroup=80,pointersOffset=0x194,followingZeroRuns=[dict(offset=0x3394,words=200),dict(offset=0x36b8,words=80)],untouchedWordsAfterThirdRun=dict(offset=0x37f8,words=120))
            assert p.read(obj,0x3e4c)==expected
            report.setdefault('ps2ConstructorPrefixes',[]).append(dict(kind=kind,**r));save()
        empty=[0]+[0xffffffff]*39;full=[0]+[0x1000+i for i in range(1,40)];cases=[]
        def add(label,table,key=0x12345678,equal=False,null_pair=False):cases.append(dict(label=label,table=table.copy(),key=key,equalRecord=equal,nullPair=null_pair))
        add('same-record',empty,equal=True);add('both-null-precedes-button',empty,0x69f878ba,null_pair=True)
        add('button',empty,0x69f878ba);add('hud-part',empty,0x7f2e3f1a);add('empty-first-free',empty);add('full-no-match',full)
        for i in (1,2,3,4,38,39):
            table=full.copy();table[i]=0x12345678;add('match-'+str(i),table)
        table=full.copy();table[1]=0xffffffff;table[38]=0x12345678;add('hole-before-match',table)
        table=full.copy();table[2]=table[39]=0x12345678;add('first-duplicate',table)
        table=full.copy();table[38]=table[39]=0xffffffff;add('last-holes',table)
        table=full.copy();table[0]=0x12345678;add('reserved-zero-slot-not-searched',table)
        table=full.copy();table[39]=0xffffffff;add('last-free-index',table)
        add('sentinel-as-query',empty,0xffffffff);add('zero-hash',empty,0);add('high-bit-hash',empty,0x80000000)
        if selection=='remaining-after17':cases=cases[17:]
        for case in cases:
            report['pending']=case;save();expected_free=0xa5a5a5a5
            if case['equalRecord'] or case['nullPair']:expected_group=0
            elif case['key']==0x69f878ba:expected_group=42
            elif case['key']==0x7f2e3f1a:expected_group=41
            else:
                expected_group=expected_free=0xffffffff
                for i in range(1,40):
                    value=case['table'][i]
                    if value==case['key']:expected_group=i;break
                    if value==0xffffffff and expected_free==0xffffffff:expected_free=i
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;manager=p.allocate(0x194);entity=p.allocate(0x28);profile=p.allocate(0x2b8);free=p.allocate(4)
            p.mu.mem_write(manager,b'\xa5'*0x194);p.mu.mem_write(entity,b'\xa5'*0x28);p.mu.mem_write(profile,b'\xa5'*0x2b8)
            p.mu.mem_write(manager+0xf4,struct.pack('<40I',*case['table']));p.put_uint(free,0xa5a5a5a5);p.put_uint(0x765ad4,profile)
            entity_record=0 if case['nullPair'] else 0x11223344;profile_record=entity_record if case['equalRecord'] or case['nullPair'] else 0x55667788
            p.put_uint(entity+0x24,entity_record);p.put_uint(profile+0x2b4,profile_record)
            snapshots=[bytes(p.mu.mem_read(a,n)) for a,n in ((manager,0x194),(entity,0x28),(profile,0x2b8))]
            group=f.call(0x573de0,manager,(entity,case['key'],free));pc=dict(group=group,firstFree=p.uint(free),blocks=sum(p.visits.values()))
            assert (group,pc['firstFree'])==(expected_group,expected_free),(case,pc)
            assert [bytes(p.mu.mem_read(a,n)) for a,n in ((manager,0x194),(entity,0x28),(profile,0x2b8))]==snapshots
            m=Ps2ScalarPrefix([(0x287bb4,0xac)]);m.map(0x21000000,0x3000);m.map(0x49f000,4096)
            manager,entity,profile,free=0x21000000,0x21001000,0x21002000,0x21002f00
            for a,n in ((manager,0x194),(entity,0x28),(profile,0x2b8)):m.write(a,b'\xa5'*n)
            m.write(manager+0xf4,struct.pack('<40I',*case['table']));m.put_uint(free,0xa5a5a5a5);m.put_uint(0x49fc7c,profile)
            m.put_uint(entity+0x24,entity_record);m.put_uint(profile+0x2b4,profile_record)
            # The real caller loads the hash with LW. R4000 64-bit register
            # state therefore sign-extends high-bit 32-bit hashes, just like
            # the classifier's own table loads. Zero extension is not the
            # original caller state and gives a false unmatched result.
            hash_register=case['key'] if case['key']<0x80000000 else case['key']|0xffffffff00000000
            for name,value in (('S2',manager),('S3',entity),('S1',hash_register),('A3',free),('GP',0x4a4170)):m.reg(name,value)
            snapshots=[m.read(a,n) for a,n in ((manager,0x194),(entity,0x28),(profile,0x2b8))]
            r=m.run(0x287bb4,[0x287c60]);r.update(group=m.reg('V0')&0xffffffff,firstFree=m.uint(free),inputHashRegister=f'{hash_register:016X}')
            assert (r['group'],r['firstFree'])==(expected_group,expected_free),(case,r)
            assert [m.read(a,n) for a,n in ((manager,0x194),(entity,0x28),(profile,0x2b8))]==snapshots
            report['cases'].append(dict(input=case,pc=pc,ps2=r));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedClassifierCases=len(cases),ps2ConstructorPrefixes=len(report.get('ps2ConstructorPrefixes',[])),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
