#!/usr/bin/env python3
"""Bounded original PC controller factories, then locally verified bindings.

No cold RTTI registration, renderer startup, OS/GPU, cap retry or controller
algorithm seam. 41A030/41A210 are distinct factories, not capped 6D1C10 init.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

FACTORIES={'uv':0x41a210,'anim':0x41a030}

def binding(mode):
    f=PCFileBytesFixture(b'');p=f.p;manager=f.call(0x454640);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    uv=mode.startswith('uv');offset=0x38 if uv else 0x64;setter=0x467d90 if uv else 0x476680
    control=f.call(FACTORIES['uv' if uv else 'anim']);holder=f.call(0x467f30)
    a=(2.,3.,0.,4.,5.,0.,6.,7.,1.);b=(8.,9.,0.,10.,11.,0.,12.,13.,1.)
    matrix=p.allocate(36);p.put_floats(matrix,a);f.call(0x467cb0,this=holder,args=(matrix,))
    if mode=='uv-flag8':p.put_uint(holder+0x30,0xabcdef08)
    f.call(setter,this=holder,args=(control,))
    check(p.uint(holder+offset)==control and (p.uint(control+8)&65535)==1,'native holder retains controller')
    check(p.uint(control+0x24)==holder,'controller material backlink')
    if uv:
        check(p.floats(control+0x28,9)==a,'UV bind snapshots current matrix')
        check(p.uint(holder+0x30)==(0xabcdef08 if mode=='uv-flag8' else 2),'UV setter state8 whole-word replacement unless bit8 set')
    if mode.endswith('alias'):
        p.put_floats(matrix,b);f.call(0x467cb0,this=holder,args=(matrix,))
        f.call(setter,this=holder,args=(control,))
        check((p.uint(control+8)&65535)==1,'alias keeps refcount')
        if uv:check(p.floats(control+0x28,9)==b and p.floats(holder+0x3c,9)==b,'UV alias binding refreshes saved matrix without restoring old snapshot')
    elif mode.endswith('rebind'):
        second=f.call(0x467f30);p.put_floats(matrix,b);f.call(0x467cb0,this=second,args=(matrix,))
        p.put_floats(holder+0x3c,tuple(-x for x in a))
        f.call(setter,this=second,args=(control,))
        check((p.uint(control+8)&65535)==2 and p.uint(control+0x24)==second,'two retained edges, backlink moves to last bound holder')
        if uv:check(p.floats(holder+0x3c,9)==a and p.floats(control+0x28,9)==b,'UV rebind restores former holder and snapshots new holder')
        f.call(p.uint(p.uint(holder)),this=holder,args=(1,));holder=second
        check(control not in f.freed and (p.uint(control+8)&65535)==1,'old holder destruction preserves shared controller')
    elif mode in ('uv-clear','uv-flag8'):
        f.call(setter,this=holder,args=(0,))
        check(p.uint(holder+offset)==0 and control in f.freed,'clear last UV owner deletes controller')
        check(p.uint(holder+0x30)==(0xabcdef08 if mode=='uv-flag8' else 0),'clear UV state8 honors preserve bit')
    capture=[mode,p.uint(holder+0x30),list(p.floats(holder+0x3c,9))]
    f.call(p.uint(p.uint(holder)),this=holder,args=(1,))
    check(control in f.freed and p.uint(manager+0x24)==p.uint(manager+0x28)==0,'last holder destroys controller and unregisters manager list')
    f.call(0x4545d0,this=manager,args=(1,))
    check(set(f.allocations)==set(f.freed),'complete binding native cleanup')
    print('CONTROLLER_BINDING_CAPTURE',capture,flush=True);print(f'PASS {checks}/{checks}: original {mode}',flush=True)
    return 0

def main(mode):
    if mode=='time-copy':
        from probe_pc_animation_lifecycle import x87_value
        f=PCFileBytesFixture(b'');p=f.p;manager=f.call(0x454640)
        first=f.call(0x41a030);second=f.call(0x41a030);states=[]
        for delta in (.5,.25,-1.,2.):
            f.call(0x423190,this=first,args=(struct.unpack('<I',struct.pack('<f',delta))[0],))
            states.append(list(p.floats(first+0x1c,2)))
        f.call(0x4231a0,this=first);elapsed=x87_value(p)
        if elapsed!=1.75 or p.floats(first+0x1c,2)!=(1.75,1.75):raise AssertionError('consume returns difference and advances applied time')
        p.put_uint(first+0x10,0);p.put_floats(first+0x1c,(2.5,7.25))
        result=f.call(0x4231d0,this=first,args=(second,))&255
        print('RENDER_CONTROLLER_COPY',result,list(p.floats(second+0x1c,2)),p.uint(second+0x10)&255,flush=True)
        if result!=1 or p.floats(second+0x1c,2)!=(2.5,7.25) or p.uint(second+0x10)&255:raise AssertionError('base copy includes enabled and both accumulated/applied times')
        for obj in (second,first):f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
        f.call(0x4545d0,this=manager,args=(1,))
        for address in (0x74e060,0x75526c,0x755264):
            owner=p.uint(address)
            if owner:f.call(p.uint(p.uint(owner)),this=owner,args=(1,))
        if set(f.allocations)!=set(f.freed):raise AssertionError('time/copy cleanup')
        print('RENDER_CONTROLLER_TIME',states,elapsed,flush=True);print('PASS 3/3: original render controller time/copy',flush=True);return 0
    if mode in ('uv-bind','uv-alias','uv-rebind','uv-clear','uv-flag8','anim-bind','anim-alias','anim-rebind'):
        return binding(mode)
    if mode not in FACTORIES:raise ValueError('uv or anim factory scout')
    f=PCFileBytesFixture(b'');p=f.p
    # Controller base requires the real global animation-manager list. Initial
    # scout omitted it and faulted at405C22 on NULL+28 (no limit was hit).
    manager=f.call(0x454640)
    obj=f.call(FACTORIES[mode]);size=f.allocations[obj]
    print('CONTROLLER_FACTORY',mode,hex(obj),'size',hex(size),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
    for offset in range(0,size,4):print(f'+{offset:03X}: {p.uint(obj+offset):08X}',flush=True)
    vt=p.uint(obj);print('VTABLE',hex(vt),[hex(p.uint(vt+i)) for i in range(0,48,4)],flush=True)
    if size!={'uv':0x1fc,'anim':0x4c}[mode]:raise AssertionError('exact original controller size')
    if p.uint(obj+0x1c) or p.uint(obj+0x20) or p.uint(obj+0x24):raise AssertionError('time and material defaults')
    if p.uint(manager+0x24)!=obj or p.uint(manager+0x28)!=obj:raise AssertionError('actual controller list registration')
    f.call(p.uint(vt),this=obj,args=(1,))
    if p.uint(manager+0x24) or p.uint(manager+0x28):raise AssertionError('controller unregisters from actual manager')
    f.call(0x4545d0,this=manager,args=(1,))
    for address in (0x75526c,0x755264):
        owner=p.uint(address)
        if owner:f.call(p.uint(p.uint(owner)),this=owner,args=(1,))
    remaining=[(hex(a),s,hex(p.uint(a))) for a,s in f.allocations.items() if a not in f.freed]
    if remaining:raise AssertionError(f'Unreleased allocations: {remaining}')
    print('PASS 6/6: original factory, defaults, registration and complete destructor',flush=True)
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
