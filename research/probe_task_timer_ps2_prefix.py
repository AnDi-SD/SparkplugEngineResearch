#!/usr/bin/env python3
"""PS2 task timer updates up to integer/FPU/child boundaries, paired with PC."""
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
    started=time.perf_counter();report=dict(kind='paired-task-timer-update-boundaries',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='PS2 fresh original scalar prefix after SQ prologue. Stop before floating conversion/arithmetic or actual child virtual Update. Source float word only loads/stores;no arithmetic. PC complete leaf Update;previous PC child traversal evidence reused,three child cases are new PS2 only. PS2 FPU rounding is explicitly not modelled.',
        limits='PC micro100k/2s;PS2 scalar2000/100ms;outer30s;whole parent/source/child guards')
    base=dict(active=1,relative=1,linked=False,child=False,now=1200,divisor=3,start=200,paused=50,current=123,sourceCurrent=0x87654321,sourceDelta=0x7fc12345)
    cases=[dict(base,**v) for v in (
        dict(active=0),dict(active=0,linked=True),dict(linked=True),dict(linked=True,sourceDelta=0x80000000),
        dict(),dict(relative=0),dict(now=0,current=0xffffffff,start=0,paused=0,divisor=1),
        dict(now=0xffffffff,current=0,start=0,paused=0,divisor=1),
        dict(now=0x80000000,current=0,start=0,paused=0,divisor=1),
        dict(now=0,current=1,start=0xffffffff,paused=7,divisor=1),
        dict(child=True,active=0,relative=255),dict(child=True,active=255,relative=0,linked=True),
        dict(child=True,active=1,relative=255,linked=True,sourceDelta=0x3f400000))]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for case in cases:
            report['pending']=case;save();pair={}
            arithmetic=bool(case['active'] and not case['linked']);now=case['now']//case['divisor']
            calculated=((now-case['start']+case['paused'])&0xffffffff) if case['relative'] else now
            difference=(calculated-case['current'])&0xffffffff
            for pc in ((False,) if case['child'] else (True,False)):
                if pc:
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;obj=p.allocate(60);source=p.allocate(60);child=p.allocate(60);f.time(case['now'],case['divisor'])
                    read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint;vt=0x6e6968
                else:
                    p=Ps2ScalarPrefix([(0x115300,0x1c0)]);p.map(0x21000000,4096);p.map(0x22000000,4096);p.map(0x49c000,0x9000)
                    obj,source,child=0x21000000,0x21000100,0x21000200;read,write,put=p.read,p.write,p.put_uint;vt=0x48cb20
                    p.map(vt,52);raw,sections=pristine();write(vt,read_window('ps2',raw,vt,52,sections)[0])
                    p.reg('A0',obj);p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
                    put(0x49c520,case['divisor']);put(0x4a2814,case['now']);write(0x4a2810,b'\0')
                write(obj,b'\xa5'*60);write(source,b'\x69'*60);write(child,b'\x96'*60);put(obj,vt);put(child,vt)
                write(obj+0x18,bytes([case['active'],case['relative']]))
                for o,v in ((0x1c,case['current']),(0x20,case['start']),(0x24,case['paused']),(0x28,0x3f800000),(0x2c,source if case['linked'] else 0),(0x30,child if case['child'] else 0)):put(obj+o,v)
                put(source+0x1c,case['sourceCurrent']);put(source+0x28,case['sourceDelta']);put(child+0x14,0)
                before=read(obj,60);source_before=read(source,60);child_before=read(child,60);expected=bytearray(before);expected_child=bytearray(child_before)
                def w(b,o,v):b[o:o+4]=struct.pack('<I',v&0xffffffff)
                if not case['active']:w(expected,0x28,0)
                elif case['linked']:w(expected,0x1c,case['sourceCurrent']);w(expected,0x28,case['sourceDelta'])
                if case['child'] and not arithmetic:expected_child[0x18:0x1a]=bytes([case['active'],case['relative']])
                if pc:
                    f.call(0x450750,obj)
                    result=dict(completion='original return',blocks=sum(p.visits.values()))
                    if arithmetic:w(expected,0x1c,calculated);expected[0x28:0x2c]=read(obj+0x28,4);result['pcDeltaBits']=f'{p.uint(obj+0x28):08X}'
                else:
                    stops=[0x1153b8,0x1153c4,0x115430,0x11543c] if arithmetic else [0x1152f0] if case['child'] else [0x1154bc,0x1154c0]
                    result=p.run(0x115300,stops)
                    if arithmetic:
                        assert p.reg('A1')&0xffffffff==calculated and p.reg('V1')&0xffffffff==difference,'integer time/difference before FPU'
                        result.update(calculatedTime=calculated,unsignedDifference=difference)
                    elif case['child']:assert p.reg('A0')==child,'actual virtual child receiver'
                assert read(obj,60)==bytes(expected),'whole parent guard'
                assert read(source,60)==source_before and read(child,60)==bytes(expected_child),'whole source/child guard'
                pair['pc' if pc else 'ps2']=result
            report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error),pending=report.get('pending'))));return 1
    print(json.dumps(dict(status='passed',ps2Cases=len(cases),pairedLeafCases=sum(not c['child'] for c in cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
