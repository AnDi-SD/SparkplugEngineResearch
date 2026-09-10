#!/usr/bin/env python3
"""Projection FX ownership, secondary BaseObject offset and bounded init rules."""
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
    started=time.perf_counter();report=dict(kind='projection-fx-native-contracts',status='running',inputs=EXPECTED,pcOwnedCases=[],ps2SetupCases=[],initCases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Two PC actual Projection factories,owned actual PCFX setup/reuse,modified geometry Clone and complete teardown. PS2 fresh setup prefix stops before allocation or executes actual no-material FX Init. Other Init tests use guarded literal dependency records,PC stops at actual helper allocation after layer writes;PS2 executes original bit transfers. No host GPU/API or fake game callback.',limits='PC micro100k/2s;PS2 scalar2000/100ms;outer30s')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def pcfixture():
        with patch.object(lifetime,'PcInstructions',PcBlocks):return TimerFixture()
    try:
        for name,factory,fields in (('spBoxProjection',0x41a150,(0x9c,0xa0,0xa4,0xa8,0xac,0xb0)),('spPyramidProjection',0x41a1b0,(0xa0,0xa4,0xa8,0xac,0xb0,0xb4,0xb8))):
            report['pending']=dict(kind='owned',name=name);save();f=pcfixture();p=f.p;obj=f.call(factory);size=f.allocations[obj];initial=bytes(p.mu.mem_read(obj,size));assert p.uint(obj+0x20)==p.uint(obj+0x94)==0
            assert f.call(0x4243d0,obj)&255==1;fx=p.uint(obj+0x94);assert fx in f.allocations and f.allocations[fx]==168
            assert p.uint(fx)==0x6f2880 and p.uint(fx+4)==0x6f2864 and p.uint(fx+0x18)==obj and p.uint(fx+0x1c)&255==1
            allocations_before=dict(f.allocations);assert f.call(0x4243d0,obj)&255==1 and p.uint(obj+0x94)==fx and f.allocations==allocations_before
            for i,o in enumerate(fields):p.put_uint(obj+o,0x3f800000+i*0x10000)
            changed=bytes(p.mu.mem_read(obj,size));clone=f.call(p.uint(p.uint(obj)+8),obj);assert clone!=obj and f.allocations[clone]==size
            assert p.uint(clone+0x94)==0 and bytes(p.mu.mem_read(obj,size))==changed
            assert all(bytes(p.mu.mem_read(clone+o,4))==initial[o:o+4] for o in fields),'derived geometry remains factory defaults after inherited Copy'
            for a in (clone,obj):f.call(p.uint(p.uint(a)),a,(1,));assert a in f.freed
            report['pending'].update(projection=obj,clone=clone,fx=fx,allocations=[dict(address=a,size=n,freed=a in f.freed) for a,n in f.allocations.items()],freed=list(f.freed));save()
            assert fx in f.freed,'owned FX freed with original Projection'
            remaining=[dict(address=a,size=n,vtable=f'{p.uint(a):08X}') for a,n in f.allocations.items() if a not in f.freed]
            assert len(remaining)==1 and remaining[0]['size']==56 and remaining[0]['vtable']=='006DC3B8','same retained context allocation as prior cold lifetime'
            report['pcOwnedCases'].append(dict(className=name,allocationBytes=size,fxAllocationBytes=168,secondaryBaseOffset=4,geometryFieldsNotCopied=[hex(o) for o in fields],setupReusesFx=True,fxAndBothProjectionsFreed=True,remainingContext=remaining,contextBoundary='Same56-byte6DC3B8 allocation remains in the prior cold lifetime;global ownership is not inferred.'));save()
        for present,flag in ((False,0),(True,0),(True,1),(True,255)):
            report['pending']=dict(kind='ps2-setup',present=present,flag=flag);save()
            q=Ps2ScalarPrefix([(0x1be3fc,0x40),(0x20adc0,0x100)]);q.map(0x21000000,4096);q.map(0x22000000,4096);q.map(0x491d70,16)
            raw,ss=pristine();q.write(0x491d70,read_window('ps2',raw,0x491d70,16,ss)[0]);obj,fx=0x21000000,0x21000200
            q.write(obj,b'\xa5'*176);q.write(fx,b'\x69'*80);q.put_uint(obj+0x20,0);q.put_uint(obj+0x8c,fx if present else 0);q.put_uint(fx,0x491d70);q.write(fx+0x1c,bytes([flag]));q.reg('A0',obj);q.reg('SP',0x22000800)
            before=q.read(obj,176);fb=q.read(fx,80);expected=bytearray(fb);r=q.run(0x1be3fc,[0x1be43c if present else 0x20b100])
            if present:expected[0x18:0x1c]=struct.pack('<I',obj);expected[0x1c]=flag or 1;assert q.reg('V0')==1
            assert q.read(obj,176)==before and q.read(fx,80)==bytes(expected),'whole PS2 setup guards'
            report['ps2SetupCases'].append(dict(present=present,initialFlag=flag,execution=r));save()
        for flag,material,count in ((1,False,0),(255,False,0),(0,False,0),(0,True,0),(0,True,1),(0,True,3)):
            report['pending']=dict(kind='init',flag=flag,material=material,count=count);save();pair={}
            for pc in (True,False):
                if pc:
                    f=pcfixture();p=f.p;fx=p.allocate(168);obj=p.allocate(208);mat=p.allocate(0x80);layers=p.allocate(0x40);entries=[p.allocate(0x20) for _ in range(3)];states=[p.allocate(0x40) for _ in range(3)]
                    read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
                else:
                    p=Ps2ScalarPrefix([(0x20adc0,0x100)]);p.map(0x21000000,0x2000);p.map(0x22000000,4096);p.reg('SP',0x22000800)
                    fx,obj,mat,layers=0x21000000,0x21000100,0x21000300,0x21000400;entries=[0x21000500+i*0x100 for i in range(3)];states=[0x21000800+i*0x100 for i in range(3)];read,write,put=p.read,p.write,p.put_uint
                size=168 if pc else 80;write(fx,b'\x69'*size);write(obj,b'\xa5'*208);write(mat,b'\x96'*0x80);write(layers,b'\x5a'*0x40)
                put(fx+0x18,obj);write(fx+0x1c,bytes([flag]));put(obj+0x20,mat if material else 0);put(mat+(0x4c if pc else 0x54),layers);put(layers+0x14,count)
                for i,(e,s) in enumerate(zip(entries,states)):write(e,b'\x69'*0x20);write(s,b'\x96'*0x40);put(e+0x10,s);put(layers+0x18+i*4,e)
                fb,ob,mb,lb=read(fx,size),read(obj,208),read(mat,0x80),read(layers,0x40);eb=[read(e,0x20) for e in entries];sb=[read(s,0x40) for s in states]
                ef,em,es=bytearray(fb),bytearray(mb),[bytearray(b) for b in sb];populated=not flag and material
                if not flag and (not material or not pc):ef[0x1c]=1
                if populated:
                    em[0x6d if pc else 0x75]=1
                    for i in range(count):
                        for o,v in ((0x1c,2),(0x20,2),(0x30,0xb if pc else 3),(0x2c,0xa if pc else 0x80)):es[i][o:o+4]=struct.pack('<I',v)
                if pc:
                    if populated:p.run(0x4c4ff0,this=fx,stop_at=0x460e50)
                    else:assert f.call(0x4c4ff0,fx)&255==1
                    r=dict(completion='actual helper allocation460E50 before remaining PC initialization' if populated else 'original return',blocks=sum(p.visits.values()))
                else:p.reg('A0',fx);r=p.run(0x20adc0,[p.RETURN]);assert p.reg('V0')==1
                assert read(fx,size)==bytes(ef) and read(obj,208)==ob and read(mat,0x80)==bytes(em) and read(layers,0x40)==lb,'whole init object/dependency guard'
                assert all(read(e,0x20)==b for e,b in zip(entries,eb)) and all(read(s,0x40)==bytes(b) for s,b in zip(states,es)),'whole layer/state guards'
                pair['pc' if pc else 'ps2']=r
            report['initCases'].append(dict(initialFlag=flag,hasMaterial=material,layerCount=count,**pair));save()
        report.pop('pending');report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error),pending=report.get('pending'))));return 1
    print(json.dumps(dict(status='passed',pcOwnedCases=2,ps2SetupCases=4,pairedInitCases=6,seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
