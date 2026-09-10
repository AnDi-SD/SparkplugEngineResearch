#!/usr/bin/env python3
"""Independent native timer integer contracts with explicit clock boundaries."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix
from capture_native_ranges import EXPECTED
import probe_pc_animation_lifecycle as lifetime


def signed_word(v):return v if v<0x80000000 else v|0xffffffff00000000


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='paired-timer-integer-common',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='PC complete methods with literal WINMM.timeGetTime samples at verified IAT6D9454;no host clock forwarding. PS2 fresh scalar prefixes stop at original clock1E7980 and separately consume declared returned samples;no wrapper/OS/FPU emulation claim. Task/game Pause and Reset original complete,Start runs through integer timer prefix. Whole object guards.',
        limits='PC micro100k/2s;PS2 scalar2000/100ms;outer30s;fresh guest per operation/prefix')
    cases=[dict(kind=k,active=a,clamp=c,limit=l,accum=acc,start=s,samples=t) for k,a,c,l,acc,s,t in (
        ('timer-start',0,0,0,77,100,[123]),('timer-start',255,255,9,0xffffffff,0,[0x80000000]),
        ('timer-reset',0,0,0,77,100,[0]),('timer-reset',1,1,9,0xffffffff,5,[0xffffffff]),
        ('timer-stop',1,0,0,70,100,[125]),('timer-stop',0,0,0,70,100,[125]),
        ('timer-stop',1,1,25,70,100,[126]),('timer-stop',1,1,25,70,100,[125,130]),
        ('timer-stop',255,255,25,70,100,[124,140]),('timer-stop',1,1,0,70,100,[101]),
        ('timer-stop',1,0,0,0xffffffff,0xffffffff,[0]),
        ('timer-stop',1,1,0xffffffff,0,0,[0x80000000,0x80000001]),
        ('timer-stop',1,1,1,0,0xffffffff,[1]),('timer-stop',0,1,25,70,100,[100,90]))]
    cases += [dict(kind=k,active=a,current=c,paused=999,delta=0x7fc12345,flag=255,now=1200,divisor=3) for k in ('task-pause','task-reset','game-pause','game-reset','game-start') for a,c in ((0,0x87654321),(255,0xffffffff))]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for case in cases:
            report['pending']=case;save();kind=case['kind'];simple=kind.startswith('timer-');size=0x88;before=bytearray(b'\xa5'*size)
            def w(b,o,v):b[o:o+4]=struct.pack('<I',v&0xffffffff)
            if simple:
                before[0x10]=case['active'];before[0x1c]=case['clamp']
                for o,v in ((0x14,case['accum']),(0x18,case['start']),(0x20,case['limit'])):w(before,o,v)
            else:
                before[0x18]=case['active'];before[0x19]=255;before[0x40]=case['flag']
                for o,v in ((0x1c,case['current']),(0x20,555),(0x24,case['paused']),(0x28,case['delta']),(0x2c,0),(0x30,0)):w(before,o,v)
            expected=bytearray(before);samples=case.get('samples',[]);expected_calls=0
            if kind in ('timer-start','timer-reset'):
                expected[0x10]=1;w(expected,0x18,samples[0]);expected_calls=1
                if kind=='timer-reset':w(expected,0x14,0)
            elif kind=='timer-stop':
                first=(samples[0]-case['start'])&0xffffffff
                clamped=bool(case['clamp'] and first>case['limit']);expected_calls=1 if not case['clamp'] or clamped else 2
                increment=case['limit'] if clamped else (samples[expected_calls-1]-case['start'])&0xffffffff
                expected[0x10]=0;w(expected,0x14,case['accum']+increment)
            elif kind.endswith('pause'):
                expected[0x18]=0;w(expected,0x24,case['current'])
                if kind=='game-pause':expected[0x40]=1
            elif kind.endswith('reset'):
                expected[0x18]=0
                for o in (0x1c,0x20,0x24):w(expected,o,0)
            else:expected[0x40]=0;expected[0x18]=1;w(expected,0x20,case['now']//case['divisor'])
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;obj=p.allocate(size);p.mu.mem_write(obj,bytes(before));clock_calls=[]
            if simple:
                # The seam is WINMM.timeGetTime itself, not either game timer method.
                for a in (0x6be2f0,0x6be2d0):p.seams.pop(a)
                target=0x34150010;p.mu.mem_map(target&~4095,4096);p.put_uint(0x6d9454,target)
                def clock(m):
                    if len(clock_calls)>=len(samples):raise AssertionError('Unexpected extra OS clock sample')
                    v=samples[len(clock_calls)];clock_calls.append(v);m.fixture_return(eax=v)
                p.seams[target]=clock
            else:f.time(case['now'],case['divisor'])
            entry={'timer-start':0x6be2d0,'timer-reset':0x6be3c0,'timer-stop':0x6be2f0,'task-pause':0x4506d0,'task-reset':0x4506e0,'game-pause':0x596900,'game-reset':0x4506e0,'game-start':0x596910}[kind]
            f.call(entry,obj);assert bytes(p.mu.mem_read(obj,size))==bytes(expected),'PC whole timer guard'
            assert len(clock_calls)==expected_calls
            pc=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()),clockSamples=clock_calls)
            segments=[];base=0x21000000
            def prefix(entry,stops,data,regs=None):
                q=Ps2ScalarPrefix([(0x1e779c,0xe8),(0x115240,0x20),(0x115260,0x74),(0x28f000,0x28)])
                q.map(base,4096);q.write(base,data);q.map(0x49c000,0x9000);q.map(0x22000000,4096)
                q.reg('GP',0x4a4170);q.reg('SP',0x22000800);q.reg('A0',base);q.reg('S0',base)
                q.put_uint(0x49c520,case.get('divisor',1));q.put_uint(0x4a2814,case.get('now',0));q.write(0x4a2810,b'\0')
                for n,v in (regs or {}).items():q.reg(n,v)
                result=q.run(entry,stops);result['declaredRegisters']={n:f'{v:016X}' for n,v in (regs or {}).items()};segments.append(result)
                return q
            if simple:
                e={'timer-start':0x1e783c,'timer-reset':0x1e786c,'timer-stop':0x1e779c}[kind]
                q=prefix(e,[0x1e7980],before);middle=bytearray(before)
                if kind=='timer-reset':w(middle,0x14,0)
                assert q.read(base,size)==bytes(middle),'PS2 pre-clock guard'
                if kind=='timer-stop':
                    e=0x1e77b0 if case['clamp'] else 0x1e7804
                    q=prefix(e,[0x1e7980,0x1e781c],middle,{'V0':signed_word(samples[0])})
                    if expected_calls==2:
                        assert q.stop==0x1e7980 and q.read(base,size)==bytes(middle),'second sample precedes mutation'
                        q=prefix(0x1e77e4,[0x1e781c],middle,{'V0':signed_word(samples[1])})
                    else:assert q.stop==0x1e781c
                else:
                    e=0x1e7844 if kind=='timer-start' else 0x1e7878
                    q=prefix(e,[0x1e7850 if kind=='timer-start' else 0x1e7884],middle,{'V0':signed_word(samples[0])})
            elif kind=='game-start':
                q=prefix(0x28f000,[0x115270],before);middle=bytearray(before);middle[0x40]=0
                assert q.read(base,size)==bytes(middle),'game Start writes pause byte before actual Task Start'
                q=prefix(0x115284,[0x1152d4],middle,{'V1':0x4a0000})
            else:
                e={'task-pause':0x115260,'task-reset':0x115240,'game-pause':0x28f010,'game-reset':0x115240}[kind]
                q=prefix(e,[Ps2ScalarPrefix.RETURN],before)
            assert q.read(base,size)==bytes(expected),'PS2 whole timer guard'
            report['cases'].append(dict(input=case,pc=pc,ps2=dict(segments=segments),guard='whole136 bytes exact',changedOffsets=[i for i,(a,b) in enumerate(zip(before,expected)) if a!=b]));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error),pending=report.get('pending'))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
