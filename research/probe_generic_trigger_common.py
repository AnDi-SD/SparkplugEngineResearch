#!/usr/bin/env python3
"""Original paired trigger cooldown, Notify dispatch and own copy payload."""
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
    started=time.perf_counter();report=dict(kind='paired-original-generic-trigger-common',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='PC full cooldown/copy and Notify until real virtual consumer. PS2 scalar cooldown prefix;complete unrelated Notify returns or real consumer stops;own copy after declared inherited success,stopping at original clone manager. No fake game callback result.',
        limits='Fresh guests;PC100k/2s,PS22000/100ms,outer30s;whole receiver/source/destination guards.')
    cases=[dict(kind='cooldown',now=n,previous=p,interval=i) for n,p,i in [(0,0,0),(1,0,0),(99,0,100),(100,0,100),(101,0,100),(3,0xfffffffd,5),(2,0xfffffffd,5),(99,100,0),(0xffffffff,0,0xffffffff),(0x80000001,0,0x80000000),(0x80000000,0,0x80000000)]]
    cases += [dict(kind='notify',code=c) for c in (0,0x1b,0x1c,0x1d,0x1e,0x1f,0x2821)]
    cases += [dict(kind='copy',bytes=b,words=w) for b,w in [([0,0,0,0],[0]*5),([1,1,0,1],[10000,1,0,100,1200]),([255,128,3,2],[0x80000000,0xffffffff,0x7fc12345,0x87654321,0x11223344])]]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for case in cases:
            report['pending']=case;save();pair={};kind=case['kind']
            for platform in ('pc','ps2'):
                if platform=='pc':
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;obj=p.allocate(0x148);size=0x148;write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
                    write(obj,b'\xa5'*size);put(obj,0x7024b0);put(obj+0x10,0);prev_off,interval_off=0x13c,0x140
                else:
                    p=Ps2ScalarPrefix([(0x3ac94c,0x6c),(0x3acc60,0x58),(0x3ad544,0x1c0-0x144)]);p.map(0x21000000,0x3000);p.map(0x49f000,0x1000);p.map(0x22000000,0x1000)
                    obj,size=0x21000000,0x160;write,read,put=p.write,p.read,p.put_uint;write(obj,b'\xa5'*size);put(obj,0x498030);put(obj+0x10,0)
                    raw,sections=pristine();p.map(0x498030,108);write(0x498030,read_window('ps2',raw,0x498030,108,sections)[0]);p.reg('SP',0x22000800);p.reg('GP',0x4a4170);p.reg('A0',obj)
                    prev_off,interval_off=0x148,0x14c
                if kind=='cooldown':
                    timer=p.allocate(0x44) if platform=='pc' else 0x21001000;write(timer,b'\0'*0x44);put(timer+0x1c,case['now']);put(0x755298 if platform=='pc' else 0x49fc80,timer)
                    put(obj+prev_off,case['previous']);put(obj+interval_off,case['interval']);before=read(obj,size)
                    if platform=='pc':value=f.call(0x590330,obj)&255;r=dict(blocks=sum(p.visits.values()),completion='original return')
                    else:r=p.run(0x3ac94c,[0x3ac9b4]);value=p.reg('V0')&0xffffffff
                    accepts=((case['now']-case['previous'])&0xffffffff)>case['interval'];expected=bytearray(before)
                    if accepts:struct.pack_into('<I',expected,prev_off,case['now'])
                    assert value==accepts and read(obj,size)==bytes(expected)
                    r.update(value=value,previous=p.uint(obj+prev_off));compare=('value','previous')
                elif kind=='notify':
                    msg=p.allocate(32) if platform=='pc' else 0x21001000;write(msg,b'\x5a'*32);put(msg,case['code']);before=read(obj,size);message_before=read(msg,32)
                    slot=15 if case['code']==0x1c else 16 if case['code']==0x1e else None
                    if platform=='pc':
                        target={15:0x5901e0,16:0x5904d0}.get(slot)
                        if target:p.run(0x590310,this=obj,args=(msg,),stop_at=target);assert p.reg('ECX')==obj
                        else:f.call(0x590310,obj,(msg,))
                        r=dict(blocks=sum(p.visits.values()),completion='actual virtual consumer entry' if target else 'original return')
                    else:
                        p.reg('A1',msg);target={15:0x3ad650,16:0x3ac9d0}.get(slot);r=p.run(0x3acc60,[target or p.RETURN]);assert not target or p.reg('A0')==obj
                    assert read(obj,size)==before and read(msg,32)==message_before;r.update(consumerSlot=slot);compare=('consumerSlot',)
                else:
                    target=p.allocate(size) if platform=='pc' else 0x21001000;write(target,b'\x5a'*size);put(target,0x7024b0 if platform=='pc' else 0x498030);put(target+0x10,0)
                    byte_offsets=(0x126,0x125,0x127,0x12c) if platform=='pc' else (0x132,0x131,0x133,0x138)
                    word_offsets=(0x134,0x140,0x13c,0x128,0x130) if platform=='pc' else (0x140,0x14c,0x148,0x134,0x13c)
                    for o,v in zip(byte_offsets,case['bytes']):write(obj+o,bytes([v]))
                    for o,v in zip(word_offsets,case['words']):put(obj+o,v)
                    before=read(target,size);source_before=read(obj,size)
                    if platform=='pc':value=f.call(0x590260,obj,(target,))&255;assert value==1;r=dict(blocks=sum(p.visits.values()),completion='original return')
                    else:
                        manager=0x21002000;put(0x49f810,manager);p.reg('S1',obj);p.reg('S0',target);r=p.run(0x3ad544,[0x104f00]);assert p.reg('A0')==manager and p.reg('A1')==0x38f1ed51
                    expected=bytearray(before)
                    for o in byte_offsets:expected[o]=source_before[o]
                    for o in word_offsets:expected[o:o+4]=source_before[o:o+4]
                    if platform=='pc':
                        for o in (0x28,0x38,0x39):expected[o]=source_before[o]
                        expected[0x3c:0x40]=source_before[0x3c:0x40]
                    assert read(target,size)==bytes(expected) and read(obj,size)==source_before,'copy guard'
                    r.update(bytes=[read(target+o,1)[0] for o in byte_offsets],words=[p.uint(target+o) for o in word_offsets]);compare=('bytes','words')
                pair[platform]=r
            assert all(pair['pc'][k]==pair['ps2'][k] for k in compare)
            report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
