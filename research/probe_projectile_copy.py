#!/usr/bin/env python3
"""Original Projectile copy payload/reference prefixes and fresh Actor PC copy."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix
from capture_native_ranges import EXPECTED
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    start=time.perf_counter();report=dict(kind='original-projectile-copy',status='running',cases=[],actorCases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='PC original Copy through allocation of new Actor;PS2 own prefix after inherited success to real allocator. Literal reference records avoid last-release destructor paths. Separate PC full Copy creates Actors via original Copy,changes source runtime inputs,then tests fresh destination defaults and real deletion/registry cleanup.',
        limits='Fresh guests;PC100k/2s,PS22000/100ms,outer30s;whole payload/reference guards')
    words=(0x10,0x28,0x34,0x38,0x40,0x44,0x48,0x4c,0x50,0x54,0x2c,0x9c);bytes_=(0x30,0xa0);refs=(0x14,0x58,0x5c,0xd4)
    definitions=[dict(label='null',old=False,new=False,same=False,count=3),dict(label='retain',old=False,new=True,same=False,count=3),dict(label='release',old=True,new=False,same=False,count=3),dict(label='replace',old=True,new=True,same=False,count=3),dict(label='same-pointer',old=True,new=True,same=True,count=3),dict(label='retain-wrap',old=False,new=True,same=False,count=0xffff)]
    def save():report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for case in definitions:
            report['pending']=case;save();pair={}
            for platform in ('pc','ps2'):
                if platform=='pc':
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;source=p.allocate(0xec);target=p.allocate(0xec);storage=p.allocate(0x200);write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
                else:
                    p=Ps2ScalarPrefix([(0x2c3474,0x210)]);p.map(0x21000000,0x3000);p.map(0x22000000,4096);source,target,storage=0x21000000,0x21001000,0x21002000;write,read,put=p.write,p.read,p.put_uint
                    p.reg('S1',source);p.reg('S0',target);p.reg('SP',0x22000800)
                write(source,b'\xa5'*0xec);write(target,b'\x5a'*0xec);write(storage,b'\x96'*0x200);put(target+0x20,0)
                for i,o in enumerate(words):put(source+o,(0x80000000,0xffffffff,0x7fc12345,0x87654321)[i%4])
                for o,v in zip(bytes_,(255,128)):write(source+o,bytes([v]))
                expected_counts=[]
                for i,o in enumerate(refs):
                    old=storage+i*0x40;new=old if case['same'] else old+0x20
                    write(old+8,case['count'].to_bytes(2,'little'));write(new+8,case['count'].to_bytes(2,'little'))
                    put(target+o,old if case['old'] else 0);put(source+o,new if case['new'] else 0)
                    expected_counts.append((old,new))
                sbefore=read(source,0xec);tbefore=read(target,0xec);rbefore=read(storage,0x200);expected=bytearray(tbefore);expected_ref=bytearray(rbefore)
                for o in words+refs:expected[o:o+4]=sbefore[o:o+4]
                for o in bytes_:expected[o]=sbefore[o]
                for old,new in expected_counts:
                    for a,delta,enabled in ((old,-1,case['old']),(new,1,case['new'])):
                        if enabled:
                            off=a-storage+8;value=int.from_bytes(expected_ref[off:off+2],'little');expected_ref[off:off+2]=((value+delta)&0xffff).to_bytes(2,'little')
                if platform=='pc':p.run(0x4ffc10,this=source,args=(target,),stop_at=0x4123d0);assert p.uint(p.reg('ESP')+4)==0x54;r=dict(blocks=sum(p.visits.values()),completion='original Actor allocator entry')
                else:r=p.run(0x2c3474,[0x10d850]);assert p.reg('A0')==0x54
                assert read(source,0xec)==sbefore and read(target,0xec)==bytes(expected) and read(storage,0x200)==bytes(expected_ref),'payload/reference whole guard'
                r.update(referenceCounts=[int.from_bytes(read(storage+i*0x20+8,2),'little') for i in range(8)],words=[p.uint(target+o) for o in words],bytes=[read(target+o,1)[0] for o in bytes_]);pair[platform]=r
            assert all(pair['pc'][k]==pair['ps2'][k] for k in ('referenceCounts','words','bytes'));report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        for existing_actor in (False,True):
            report['pending']=dict(kind='pc-actor-copy',existingTargetActor=existing_actor);save()
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;manager=f.call(0x454640);source=f.call(0x4018e0);target=f.call(0x4018e0)
            assert f.call(0x4ffc10,target,(source,))&255==1;actor=p.uint(source+0x20);assert actor and p.uint(actor)==0x703f80
            if existing_actor:assert f.call(0x4ffc10,source,(target,))&255==1
            old=p.uint(target+0x20);p.mu.mem_write(actor+0x1c,b'\0');p.mu.mem_write(actor+0x24,b'\0');p.put_uint(actor+0x20,0x3f000000)
            before=bytes(p.mu.mem_read(source,0xec));assert f.call(0x4ffc10,source,(target,))&255==1;new=p.uint(target+0x20)
            assert new not in (0,actor,old) and p.uint(new)==0x703f80 and (not old or old in f.freed)
            assert bytes(p.mu.mem_read(source,0xec))==before and p.uint(actor+0x20)==0x3f000000 and p.uint(new+0x20)==0x3f800000
            assert bytes(p.mu.mem_read(new+0x1c,1))==bytes(p.mu.mem_read(new+0x24,1))==b'\1'
            assert p.uint(manager+0x24)==actor and p.uint(manager+0x28)==new and p.uint(actor+0x14)==new and p.uint(new+0x18)==actor
            for obj in (target,source):f.call(p.uint(p.uint(obj)),obj,(1,));assert obj in f.freed
            assert actor in f.freed and new in f.freed and p.uint(manager+0x24)==p.uint(manager+0x28)==0
            f.call(0x4545d0,manager,(1,));assert manager in f.freed and p.uint(0x75f880)==0
            report['actorCases'].append(dict(existingTargetActor=existing_actor,oldActorFreed=bool(old and old in f.freed),freshActorDefaults=True,sourceActorSettingsPreserved=True,emptyRegistryAndManagerDeleted=True,blocks=sum(p.visits.values())));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(report['cases']),pcActorCases=len(report['actorCases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
