#!/usr/bin/env python3
"""Bounded standalone TransFunction and UV clone/clear observations."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture
from probe_pc_function_eval import cleanup
from probe_pc_uv_functions import OFFSETS

def main(mode,return_capture=False):
    if mode not in ('trans-factory','trans-clone','uv-clear-changed','uv-clone-unbound','uv-clone-bound','uv-clone-mapped'):raise ValueError('bounded scalar ownership case')
    f=PCFileBytesFixture(b'');p=f.p;checks=0;captures=[]
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    manager=f.call(0x454640);objects=[]
    if mode.startswith('trans-'):
        obj=f.call(0x47d2e0);objects=[obj]
        check(f.allocations[obj]==0x1b0 and p.uint(obj)==0x6eaaec,'actual standalone TransFunction factory1B0')
        if mode=='trans-clone':
            f.call(0x52fd90,this=0x755588)
            p.put_floats(obj+0x160,(1,2,3));p.put_floats(obj+0x10+0x24,(7,));p.put_uint(obj+0x10+0x34,8)
            clone=f.call(0x47d340,this=obj);objects.insert(0,clone)
            captures=[list(p.floats(clone+0x160,6)),[p.floats(clone+offset+0x24,1)[0] for offset in OFFSETS],p.uint(clone+0x10+0x34)]
            check(captures==[[0,0,0,0,0,1],[0,0,0,1,1,1,0],0],'standalone Trans virtual clone keeps factory defaults, not UV embedded assignment')
            f.call(0x6d7db0)
    else:
        uv=f.call(0x41a210);objects=[uv];holder=0;other=0
        a=(2.,3.,0.,4.,5.,0.,6.,7.,1.);b=(8.,9.,0.,10.,11.,0.,12.,13.,1.)
        if mode!='uv-clone-unbound':
            holder=f.call(0x467f30);objects=[holder];matrix=p.allocate(36);p.put_floats(matrix,a)
            f.call(0x467cb0,this=holder,args=(matrix,));f.call(0x467d90,this=holder,args=(uv,))
        if mode=='uv-clear-changed':
            p.put_floats(holder+0x3c,b);f.call(0x467d90,this=holder,args=(0,))
            captures=list(p.floats(holder+0x3c,9));check(uv in f.freed,'NULL clear destroys last retained UV');
            check(captures==list(b),'UV destructor does not restore saved baseline; rebind is separate')
        else:
            f.call(0x52fd90,this=0x755588)
            clone_manager=f.call(0x412540);p.put_uint(0x74e060,clone_manager)
            if mode=='uv-clone-mapped':
                other=f.call(0x467f30);objects.append(other)
                f.call(0x412f70,this=clone_manager,args=(holder,other))
            p.put_floats(uv+0x1c,(2,3));p.put_floats(uv+0x4c+0x160,(1,2,3))
            for i,offset in enumerate(OFFSETS):
                p.put_floats(uv+0x4c+offset+0x10,(i+.25,));p.put_floats(uv+0x4c+offset+0x24,(i+4.,));p.put_uint(uv+0x4c+offset+0x34,8)
            clone=f.call(0x41abb0,this=uv);objects.insert(0,clone)
            linked=p.uint(clone+0x24)
            print('UV_CLONE_LINK',mode,hex(holder),hex(other),hex(linked),'allocation',f.allocations.get(linked),'cloneRefs',p.uint(clone+8)&65535,flush=True)
            if mode=='uv-clone-bound':
                check(linked not in (0,holder) and f.allocations.get(linked)==0x68,'unmapped nonnull material is recursively cloned, not merely looked up')
                linked_uv=p.uint(linked+0x38)
                print('UV_CLONED_HOLDER_EDGE',hex(linked_uv),'sourceUV',hex(uv),'rootClone',hex(clone),'edgeAllocation',f.allocations.get(linked_uv),flush=True)
                check(linked_uv not in (0,uv,clone) and f.allocations.get(linked_uv)==0x1fc
                      and (p.uint(linked_uv+8)&65535)==1 and (p.uint(clone+8)&65535)==0,
                      'material copy always-clones its UV via412BE0, creating a second retained controller despite existing root mapping')
                check(p.uint(linked_uv+0x24)==linked,'nested UV material backlink is the owning clone holder')
                objects.insert(0,linked)
            else:check(linked==other,'null or pre-mapped material resolves as supplied')
            check(p.floats(clone+0x1c,2)==(2.,3.),'UV clone copies both RenderController clocks')
            check(p.floats(clone+0x28,9)==p.floats(uv+0x28,9),'UV saved baseline copied')
            for offset in OFFSETS:
                check(bytes(p.mu.mem_read(clone+0x4c+offset+0x10,0x28))==bytes(p.mu.mem_read(uv+0x4c+offset+0x10,0x28)),'embedded scalar state copied including time/type/clamp bytes')
            check(p.floats(clone+0x4c+0x160,6)==p.floats(uv+0x4c+0x160,6),'embedded pivot and axis copied')
            captures=[bool(other),list(p.floats(clone+0x1c,2)),list(p.floats(clone+0x28,9)),[list(p.floats(clone+0x4c+offset+0x10,8))+[p.uint(clone+0x4c+offset+0x34)] for offset in OFFSETS]]
            f.call(0x6d7db0)
    for obj in objects:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(p.uint(manager+0x24)==p.uint(manager+0x28)==0,'all controller nodes unregistered')
    f.call(0x4545d0,this=manager,args=(1,));cleanup(f,());check(set(f.allocations)==set(f.freed),'all actual allocations released')
    print('UV_LIFETIME_CAPTURE',json.dumps([mode,captures]),flush=True);print(f'PASS {checks}/{checks}: original {mode}; heap={p.allocated}',flush=True)
    return [mode,captures] if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
