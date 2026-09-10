#!/usr/bin/env python3
"""Paired original trigger Tick control flow with explicit predicate inputs."""
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
    started=time.perf_counter();report=dict(kind='paired-original-generic-trigger-tick-control-flow',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Literal guarded receiver/input/HUD/GameFlow records. V19 predicate explicitly selects an original constant leaf;no geometry claim. Other vtable entries are pristine. Original HUD query and GameFlow reverse-stack lookup execute;entry/leave/activation callbacks stop at real consumers. PC complete no-callback branches;PS2 prefix before SQ/LQ restore.',
        limits='Fresh PC100k/2s and PS22000/100ms guests;30s outer;whole receiver and borrowed dependency guards.')
    common=dict(single=0,fired=0,active=0,latch=0,input=0,controller=1,busy=0,kindMatch=1,predicate=0,due=0)
    definitions=[('already-fired',dict(single=1,fired=1,due=1)),('idle-rearm',{}),('active-rearm',dict(active=1)),
        ('input-held-controller-disabled',dict(active=1,latch=1,input=1,controller=0)),('input-held-latch-zero',dict(active=1,input=1)),
        ('activate',dict(active=1,input=1,latch=1)),('activate-kind-mismatch',dict(active=1,input=1,latch=1,kindMatch=0)),
        ('due-rejected-inactive',dict(due=1)),('due-rejected-active',dict(due=1,active=1)),
        ('due-accepted-hud-busy',dict(due=1,predicate=1,busy=1)),('due-accepted-kind-mismatch',dict(due=1,predicate=1,kindMatch=0)),
        ('enter',dict(due=1,predicate=1)),('repeat-enter',dict(due=1,predicate=1,active=1)),('single-not-yet-fired',dict(single=1,due=1,predicate=1))]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for label,changes in definitions:
            case=dict(common,**changes);case['label']=label;report['pending']=case;save();pair={}
            skip=case['single'] and case['fired'];predicate_count=0 if skip or not case['due'] else 2 if not case['predicate'] and case['active'] else 1
            callback=None;active=case['active'];latch=case['latch']
            if not skip:
                if case['due'] and not case['predicate'] and active:active=0;callback='leave'
                elif case['due'] and case['predicate'] and not case['busy'] and case['kindMatch']:active=1;callback='enter'
                if callback is None:
                    if active and case['input'] and case['controller'] and latch and case['kindMatch']:latch=0;callback='activate'
                    elif not case['input']:latch=1
            for platform in ('pc','ps2'):
                if platform=='pc':
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;allocate=p.allocate;write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
                    obj=allocate(0x148);size=0x148;vt=allocate(100);write(vt,read(0x7024b0,100));put(vt+19*4,0x4f3df0 if case['predicate'] else 0x4a1bf0)
                    offsets=dict(single=0x125,fired=0x127,active=0x126,latch=0x145,previous=0x13c,interval=0x140,input=0x134)
                    timer,hud,hud_state,flow,controller,input_record=allocate(0x44),allocate(0x64),allocate(0x150),allocate(0x1b4),allocate(0x28),allocate(0x20)
                    globals_=(0x755298,0x755284,0x755294);hud_flag=0x144
                else:
                    p=Ps2ScalarPrefix([(0x3ac9e0,0x26c),(0x3ac710,0x18),(0x2c8df0,8),(0x379470,12),(0x339ba0,0x58)]);p.map(0x21000000,0x6000);p.map(0x22000000,0x1000);p.map(0x49f000,0x1000)
                    obj,size,vt=0x21000000,0x160,0x21000800;write,read,put=p.write,p.read,p.put_uint;raw,sections=pristine();write(vt,read_window('ps2',raw,0x498030,108,sections)[0]);put(vt+8+19*4,0x3ac720 if case['predicate'] else 0x2c8df0)
                    offsets=dict(single=0x131,fired=0x133,active=0x132,latch=0x151,previous=0x148,interval=0x14c,input=0x140)
                    timer,hud,hud_state,flow,controller,input_record=0x21001000,0x21002000,0x21003000,0x21004000,0x21005000,0x21005100
                    globals_=(0x49fc80,0x49fda4,0x49fd4c);hud_flag=0x14c;p.reg('GP',0x4a4170);p.reg('SP',0x22000800);p.reg('A0',obj)
                write(obj,b'\xa5'*size);put(obj,vt)
                for k in ('single','fired','active','latch'):write(obj+offsets[k],bytes([case[k]]))
                put(obj+offsets['previous'],1000);put(obj+offsets['interval'],100);put(obj+offsets['input'],input_record)
                dependencies={timer:0x44,hud:0x64,hud_state:0x150,flow:0x1b4,controller:0x28,input_record:0x20}
                for a,n in dependencies.items():write(a,b'\0'*n)
                put(timer+0x1c,1101 if case['due'] else 1100);put(hud+0x60,hud_state);write(hud_state+hud_flag,bytes([case['busy']]))
                put(flow+0x1ac,0);put(flow+0x15c,controller);put(flow+0x1b0,5 if case['kindMatch'] else 6);put(controller+0x10,5)
                write(controller+0x25,bytes([case['controller']]));write(input_record+0x1b,bytes([case['input']]))
                for g,a in zip(globals_,(timer,hud,flow)):put(g,a)
                before=read(obj,size);dep_before={a:read(a,n) for a,n in dependencies.items()};observed=[]
                if platform=='pc':
                    def observe(mu,address,n,user):
                        if address in (0x4f3df0,0x4a1bf0) and p.uint(p.reg('ESP')) in (0x590527,0x590592):observed.append('predicate')
                    hook=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
                    target={'enter':0x5903b0,'leave':0x590420,'activate':0x590370}.get(callback)
                    if target:p.run(0x5904d0,this=obj,stop_at=target);assert p.reg('ECX')==obj
                    else:assert f.call(0x5904d0,obj)&255==1
                    p.mu.hook_del(hook);r=dict(blocks=sum(p.visits.values()),completion='actual '+callback+' callback entry' if callback else 'original return')
                else:
                    def observe(mu,address,n,user):
                        if address in (0x3ac720,0x2c8df0):observed.append('predicate')
                    p.u.hook_add(__import__('unicorn').UC_HOOK_CODE,observe);target={'enter':0x3ac780,'leave':0x3ac730,'activate':0x3ac830}.get(callback)
                    r=p.run(0x3ac9e0,[target or 0x3acc4c]);assert not target or p.reg('A0')==obj
                expected=bytearray(before);expected[offsets['active']]=active;expected[offsets['latch']]=latch
                if case['due'] and not skip:struct.pack_into('<I',expected,offsets['previous'],1101)
                assert read(obj,size)==bytes(expected),'receiver guard'
                assert all(read(a,len(b))==b for a,b in dep_before.items()),'dependency guard'
                assert len(observed)==predicate_count,(case,len(observed),predicate_count)
                r.update(callback=callback,predicateCalls=len(observed),active=active,latch=latch,previous=p.uint(obj+offsets['previous']));pair[platform]=r
            assert all(pair['pc'][k]==pair['ps2'][k] for k in ('callback','predicateCalls','active','latch','previous'))
            report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(definitions),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
