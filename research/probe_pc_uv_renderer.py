#!/usr/bin/env python3
"""Original PC material UV -> renderer slots23/24 -> declared COM SetTransform.

Explicit consumer-only renderer storage fits the original64KiB arena; no
renderer constructor, texture loader, GPU, OS, or capped path is invoked.
Only the external device call is observed. Native allocation cap stays32KiB.
"""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup,configure_floor,bits


def main(mode,return_capture=False):
    if mode not in ('none','static','static-fail','uv-idle','uv-active','uv-shared','uv-both','anim-only','matrix3','matrix4','matrix4-fail'):raise ValueError('tiny two-stage boundary')
    f=PCWriteBytesFixture();p=f.p;configure_floor(f);checks=0;events=[];updates=[];anim=0;phases=[];submit_phase=None;animation_updates=[]
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    # Declared backing storage, not native allocation/constructor evidence.
    # 0xF174 contains precisely the two observed stage caches at F0F4/F134.
    renderer=p.allocate(0xf174);device=p.allocate(4);table=p.allocate(0xb4)
    p.put_uint(renderer+0x18,0x6f28a0);p.put_uint(renderer+0xc9e8,device);p.put_uint(device,table);p.put_uint(0x75db68,renderer)
    check(p.uint(0x6f28a0+23*4)==0x4bb590 and p.uint(0x6f28a0+24*4)==0x4bb4b0,'original PC renderer platform entries')
    p.mu.mem_map(0x34090000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC);p.put_uint(table+0xb0,0x34090010)
    def submit(machine):
        nonlocal submit_phase
        sp=machine.reg('ESP');owner=machine.uint(sp+4);stage=machine.uint(sp+8);matrix=machine.uint(sp+12)
        if owner!=device or stage not in (16,17) or len(events)>=8:raise AssertionError('two-stage finite COM observer')
        events.append([stage,list(machine.floats(matrix,16))]);machine.fixture_return(12,eax=0x80004005 if mode.endswith('-fail') else 0)
        if anim:submit_phase=p.floats(anim+0x48,1)[0]
    p.seams[0x34090010]=submit
    p.mu.hook_add(p.uc.UC_HOOK_CODE,lambda machine,address,size,unused:updates.append(address) if address==0x434820 else None)
    p.mu.hook_add(p.uc.UC_HOOK_CODE,lambda machine,address,size,unused:animation_updates.append(address) if address==0x42fc60 else None)
    animations=f.call(0x454640);holder=f.call(0x467f30);objects=[holder];uv=0;second=0
    matrix=p.allocate(64);values=tuple(float(i) for i in range(1,17));p.put_floats(matrix,values)
    if mode in ('static','static-fail'):f.call(0x467cb0,this=holder,args=(matrix,))
    if mode.startswith('uv-'):
        uv=f.call(0x41a210);f.call(0x467d90,this=holder,args=(uv,))
        p.put_uint(uv+0x4c+0x10+0x34,8);p.put_floats(uv+0x4c+0x10+0x28,(2.,))
        if mode=='uv-shared':
            second=f.call(0x467f30);objects.append(second);f.call(0x467d90,this=second,args=(uv,))
    if mode in ('uv-both','anim-only'):
        anim=f.call(0x41a030);p.run(0x412400,args=(4,),callee_pop=False);times=p.reg('EAX');p.run(0x412400,args=(8,),callee_pop=False);slots=p.reg('EAX')
        p.put_floats(times,(1.,));p.put_uint(slots,1);p.put_uint(slots+4,0)
        p.put_uint(anim+0x28+0x14,times);p.put_uint(anim+0x28+0x18,slots+4);p.put_uint(anim+0x28+0x1c,1)
        f.call(0x476680,this=holder,args=(anim,))
    states=[];maximum=0
    for step,delta in enumerate((1.,0.,.5)):
        submit_phase=None
        if uv and mode!='uv-idle':f.call(0x423190,this=uv,args=(bits(delta),))
        if anim:f.call(0x423190,this=anim,args=(bits(delta),))
        if mode.startswith('matrix'):
            slot=23 if mode.startswith('matrix4') else 24
            result=f.call(p.uint(0x6f28a0+4*slot),this=renderer+0x18,args=(step%2,matrix))&255
            check(result==1,'backend returns true including failed COM HRESULT')
        else:f.call(0x467b70,this=holder,args=(step%2,))
        maximum=max(maximum,sum(p.visits.values()))
        state=[list(p.floats(holder+0x3c,9))]
        if second:state.append(list(p.floats(second+0x3c,9)))
        states.append([state,[list(p.floats(renderer+0xf0f4+64*i,16)) for i in range(2)]])
        if anim:phases.append([submit_phase,p.floats(anim+0x48,1)[0]])
    check(len(events)==(0 if mode in ('none','anim-only') else 3),'no static/UV means no submit; otherwise every call submits with no cache dedup')
    check(len(updates)==(2 if mode in ('uv-active','uv-shared','uv-both') else 0),'equal UV clocks skip recompute but do not suppress submission')
    if anim:check(len(animation_updates)==3 and phases==([[0.,1.],[1.,1.],[1.,.5]] if uv else [[None,1.],[None,1.],[None,.5]]),'UV submission precedes unconditional AnimTex including equal clocks')
    if mode=='uv-shared':check(states[-1][0][0][6]==0 and states[-1][0][1][6]==3,'first holder submits its unchanged matrix while shared UV updates last-bound holder')
    if events:check(events[-1][1]==states[-1][1][0],'cache and COM4x4 match for last stage0')
    for obj in objects:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(not uv or uv in f.freed,'UV owner lifecycle ends exactly once')
    check(p.uint(animations+0x24)==p.uint(animations+0x28)==0,'all native controller nodes removed')
    f.call(0x4545d0,this=animations,args=(1,));cleanup(f,());check(set(f.allocations)==set(f.freed),'all actual allocations freed; declared renderer backing is not a native owner')
    capture=[mode,states,events,len(updates)]
    if anim:capture.append(phases)
    print('UV_RENDERER_CAPTURE',json.dumps(capture),flush=True);print(f'PASS {checks}/{checks}: original material/renderer {mode}; heap={p.allocated}; maxInstructions={maximum}',flush=True)
    return capture if return_capture else 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
