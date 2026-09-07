#!/usr/bin/env python3
"""PC declaration map/factory/layout boundaries, bounded guest without Direct3D."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture

checks=0
def check(v,label):
    global checks
    checks+=1
    if not v:raise AssertionError(label)

def main(mode):
    if mode not in {'constructor','map-hit','map-null','map-miss','layout-0','layout-840','layout-940','layout-20','layout-93e','layout-1fffff'}:
        raise ValueError('explicit bounded declaration mode required')
    f=LifetimeFixture();p=f.p
    obj=f.call(0x4c9c20)
    print('DECL FACTORY',hex(obj),'size',f.allocations[obj],'words',f.words(obj,0,f.allocations[obj]),'instructions',sum(p.visits.values()),flush=True)
    check(f.allocations[obj]==0x1c and p.uint(obj)==0x6f2e58,'actual spPCVertexDeclaration factory extent/vtable')
    check(p.uint(obj+0x18)==0,'COM declaration pointer starts null')
    if mode.startswith('map-'):
        renderer=p.allocate(0xf364);head=p.allocate(24);node=p.allocate(24)
        p.put_uint(renderer+0xf35c,head);p.put_uint(renderer+0xf360,0 if mode=='map-miss' else 1)
        for off in (0,4,8):
            p.put_uint(head+off,head if mode=='map-miss' else node);p.put_uint(node+off,head)
        p.mu.mem_write(head+20,b'\x01\x01');p.mu.mem_write(node+20,b'\x01\x00')
        p.put_uint(node+12,0x840);p.put_uint(node+16,0 if mode=='map-null' else obj)
        p.run(0x4ae0e0,this=renderer,args=(0x840,),stop_at=0x4ae111)
        if mode=='map-miss':
            check(p.reg('EIP')==0x4ae111,'empty map stops before declaration factory and backend work')
            p.put_uint(f.teb,0xffffffff)
        else:
            check(p.reg('EAX')==(0 if mode=='map-null' else obj),'map returns stored declaration pointer including null')
            check(0x4c9c20 not in p.visits and 0x4c9d20 not in p.visits,'cache hit bypasses factory/Initialize, not a numeric FVF handle')
        print('DECL MAP',mode,'result',hex(p.reg('EAX')),'eip',hex(p.reg('EIP')),'instructions',sum(p.visits.values()),flush=True)
    if mode.startswith('layout-'):
        flags=int(mode.split('-',1)[1],16)
        result=f.call(0x4b23c0,this=obj,args=(flags,))&255
        print('DECL BASE INIT',hex(flags),result,f.words(obj,0x10,0x1c),'instructions',sum(p.visits.values()),flush=True)
        p.run(0x4c9a00,args=(flags,),callee_pop=False);array=p.reg('EAX')
        check(array in f.allocations,'original declaration builder returns allocated array')
        size=f.allocations[array];check(size<=256 and size%8==0,'bounded eight-byte declaration elements')
        raw=bytes(p.mu.mem_read(array,size))
        print('DECL ELEMENTS',hex(flags),'size',size,'hex',raw.hex(),'instructions',sum(p.visits.values()),flush=True)
        terminals=[i for i in range(0,size,8) if raw[i:i+8]==bytes.fromhex('ff00000011000000')]
        check(len(terminals)==1,'native declaration terminator precedes possible unused allocation tail')
        check(all(b==0xcc for b in raw[terminals[0]+8:]),'overallocated tail remains uninitialized, not declaration elements')
        p.run(0x412420,args=(array,),callee_pop=False)
    f.call(0x4c9ce0,this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all tracked declaration allocations freed')
    print(f'PASS {checks}/{checks}: PC declaration {mode}; no graphics device')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
