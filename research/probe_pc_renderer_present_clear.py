#!/usr/bin/env python3
"""Original PC Clear/Present finite COM boundaries; never runs device reset.

Present reset-needed case STOPS before native internal slot8 at4BBA50 and
inspects its real argument. It is not a successful reset/whole-Present proof.
"""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup

def main(mode,return_capture=False):
    if mode not in ('clear','clear-fail','present','present-fail','blocked','lost','lost-reset-stop'):raise ValueError('bounded no-reset renderer boundary')
    f=PCWriteBytesFixture();p=f.p;renderer=p.allocate(0xca00);device=p.allocate(4);table=p.allocate(0xb0);events=[];states=[];checks=0;maximum=0
    p.put_uint(renderer+0x18,0x6f28a0);p.put_uint(renderer+0xc9e8,device);p.put_uint(device,table)
    if mode=='blocked':p.put_uint(renderer+0xc1c0,1)
    p.mu.mem_map(0x34090000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def clear(machine):
        sp=machine.reg('ESP');values=[machine.uint(sp+4+4*i) for i in range(7)]
        check(values[0]==device and values[1:3]==[0,0] and values[5]==0x3f800000,'COM Clear this/null rectangles/fixed depth1')
        if len(events)>=264:raise AssertionError('fixed Clear case bound')
        events.append(['clear',values[3],values[4],values[5],values[6]]);machine.fixture_return(28,eax=0x80004005 if mode=='clear-fail' else 0)
    def present(machine):
        sp=machine.reg('ESP');check(machine.uint(sp+4)==device and all(machine.uint(sp+i)==0 for i in (8,12,16,20)),'COM Present null rectangles/window/dirty region')
        events.append(['present']);machine.fixture_return(20,eax=0x88760868 if mode.startswith('lost') else 0x80004005 if mode=='present-fail' else 0)
    def cooperative(machine):
        check(machine.uint(machine.reg('ESP')+4)==device,'COM cooperative-level this');events.append(['cooperative']);machine.fixture_return(4,eax=0x88760869 if mode=='lost-reset-stop' else 0x88760868)
    for slot,address,callback in ((0xac,0x34090010,clear),(0x44,0x34090020,present),(0x0c,0x34090030,cooperative)):
        p.put_uint(table+slot,address);p.seams[address]=callback
    if mode.startswith('clear'):
        flags=list(range(256))+[0x100,0x107,0x80000000,0xffffffff]
        for flag in flags:
            result=f.call(0x4bb980,this=renderer+0x18,args=(flag,0x11223344,0x55667788))&255;maximum=max(maximum,sum(p.visits.values()))
            check(result==1 and events[-1][1:]==[flag&7,0x11223344,0x3f800000,0x55667788],'all low-byte flags mapped only bits0..2; HRESULT ignored')
            states.append([flag,result])
    elif mode=='lost-reset-stop':
        params=[0x100+i for i in range(8)]
        for i,value in enumerate(params):p.put_uint(renderer+0x1c+4*i,value)
        # Complete params end3B, device pointer unaffected.
        p.run(0x4bb9f0,this=renderer+0x18,args=(1,),stop_at=0x4bba50);maximum=sum(p.visits.values())
        pointer=p.uint(p.reg('ESP'));copied=[p.uint(pointer+4*i) for i in range(8)]
        check(p.reg('ECX')==renderer+0x18 and p.uint(p.reg('EAX')+0x20)==0x4bd810 and copied==params,'stop BEFORE internal reset/slot8, actual stack32-byte argument copy')
        states=[['stopped',copied]] # never resumed, no successful native result claimed
    else:
        result=f.call(0x4bb9f0,this=renderer+0x18,args=(1,))&255;maximum=sum(p.visits.values())
        check(result==(0 if mode=='blocked' else 1),'blocked flag false, other bounded Present returns true')
        check(len(events)==(0 if mode=='blocked' else 2 if mode=='lost' else 1),'original Present/cooperative call count')
        states=[result]
    cleanup(f,());check(set(f.allocations)==set(f.freed),'no leaked actual native allocations')
    capture=[mode,events,states];print('RENDERER_PRESENT_CLEAR_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original PC boundary {mode}; maxInstructions={maximum}; heap={p.allocated}; resetExcluded=true',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
