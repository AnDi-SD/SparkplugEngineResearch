#!/usr/bin/env python3
"""Original PC scene-owned projection/flare lifecycle and dispatch boundaries.

Actual constructor/destructor/list/clone/reset calls execute. Projection helper,
projection draw and D3D capability query are explicit recording interfaces.
No COM object, device, OS or real GPU query is created by the host.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_special_registry import SpecialSceneFixture,list_entries,flare_entries
from probe_pc_san_reader import cstring

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

def lifecycle_and_name_clone():
    f=SpecialSceneFixture();p=f.p
    def release_shared_name(p):
        slot=p.reg('ECX');name=p.uint(slot)
        if name:
            check(name in f.allocations and name not in f.freed,'bounded shared name fixture ownership')
            count=bytes(p.mu.mem_read(name+8,1))[0]
            check(0<count<255,'bounded positive shared name fixture count')
            if count==1:f.freed.append(name)
            else:p.mu.mem_write(name+8,bytes((count-1,)))
            p.put_uint(slot,0)
        p.fixture_return()
    p.seams[0x4173e0]=release_shared_name
    p.seams[0x412f70]=lambda p:p.fixture_return(8)
    for factory,clone_entry,size,table in ((0x4c5ce0,0x4c5d50,0x24,0x6f2a00),
                                         (0x4c7240,0x4c72a0,0x38,0x6f2a5c)):
        manager=f.call(factory)
        check(f.allocations[manager]==size and p.uint(manager)==table,'actual PC manager allocation/table')
        check(p.uint(manager+0x10)==0 and p.uint(manager+0x14)==0xcccccc00,
              'inherited name empty, native enabled false and padding untouched')
        name=p.allocate(16);p.mu.mem_write(name,b'manager-test\0')
        f.call(0x4130f0,this=manager,args=(name,))
        obj=p.allocate(0x100)
        if size==0x24:f.call(0x4cdf60,this=manager,args=(obj,))
        else:f.call(0x4c5dc0,this=manager,args=(obj,))
        p.mu.mem_write(manager+0x14,b'\1')
        clone=f.call(clone_entry,this=manager)
        check(p.uint(clone+0x10)==p.uint(manager+0x10) and
              bytes(p.mu.mem_read(p.uint(manager+0x10)+8,1))==b'\2',
              'actual inherited PC name copy shares entry and increments byte refcount')
        check(cstring(p,p.uint(clone+0x10)+9)==b'manager-test' and
              bytes(p.mu.mem_read(clone+0x14,1))==b'\0',
              'native manager clone copies only name, not enabled state')
        check((list_entries(p,clone) if size==0x24 else flare_entries(p,clone))==[],
              'native manager clone leaves borrowed registry empty')
        for value in (clone,manager):f.call(p.uint(p.uint(value)),this=value,args=(1,))
        check(p.uint(obj+8)==0,'manager destructors do not release borrowed payload')
    f.close()

def projection_dispatch():
    f=SpecialSceneFixture();p=f.p
    manager=f.call(0x4c5ce0)
    node=f.render_node()
    projections=f.borrowed_renderables(node,(0x32bb2f56,0x1cca7732,0x58da4026))
    calls=[]
    camera=p.allocate(0x238)
    helpers=[]
    results=(1,0,3)
    for i,obj in enumerate(projections):
        helper,table=p.allocate(0x20),p.allocate(4)
        p.put_uint(helper,table);p.put_uint(obj+0x94,helper);helpers.append(helper)
        init_entry,draw_entry=0x340a0800+i*0x20,0x340a0810+i*0x20
        p.put_uint(table,init_entry);p.put_uint(p.uint(obj)+0x38,draw_entry)
        def initialize(p,obj=obj,helper=helper):
            check(p.reg('ECX')==helper and p.uint(helper+0x18)==obj,
                  'actual projection4243D0 sets helper owner before virtual0')
            calls.append(('init',obj));p.fixture_return(eax=0)
        def draw(p,obj=obj,result=results[i]):
            check(p.reg('ECX')==obj and p.uint(p.reg('ESP')+4)==camera,'projection late draw virtual38 argument')
            calls.append(('draw',obj));p.fixture_return(4,eax=result)
        p.seams[init_entry]=initialize;p.seams[draw_entry]=draw
        f.call(0x4cdf60,this=manager,args=(obj,))
    check(f.call(0x45a290,this=manager)&255==1 and f.call(0x4c5ca0,this=manager)&255==1,
          'projection Init only enables manager')
    check(f.call(0x4cde70,this=manager)&255==1 and calls==[('init',o) for o in projections],
          'all original projection init wrappers ignore helper return and return true')
    calls.clear()
    check(f.call(0x4cdea0,this=manager,args=(camera,))&255==0 and
          calls==[('draw',o) for o in projections], 'late draw combines low bytes without short circuit')
    f.call(0x4c5cb0,this=manager,args=(0,));calls.clear()
    f.call(0x4cdea0,this=manager,args=(camera,))
    check(calls==[('draw',o) for o in projections], 'manager flag is checked by Scene caller, not late draw loop')
    check(f.call(0x5a7db0,this=manager,args=(camera,))&255==1,'PC first two projection phases are original no-op true')
    f.call(0x4c5da0,this=manager,args=(1,))
    for off in (0xbc,0xc0,0xc4):p.put_uint(node+off,0)
    f.call(0x4255d0,this=node,args=(1,))
    f.close()

def flare_capability_init():
    f=SpecialSceneFixture();p=f.p
    manager=f.call(0x4c7240)
    device,table=p.allocate(4),p.allocate(0x1dc)
    p.put_uint(device,table);p.put_uint(table+0x1d8,0x340a0c00)
    p.put_uint(f.renderer+0xc9e8,device)
    status=0
    def query(p):
        check(tuple(p.uint(p.reg('ESP')+4*i) for i in (1,2,3))==(device,9,0),
              'original D3D query capability call passes device/type9/null output')
        p.fixture_return(12,eax=status)
    p.seams[0x340a0c00]=query
    for status in (0,0x8876086a,0x80004005):
        check(f.call(0x4c5e30,this=manager)&255==1 and
              bytes(p.mu.mem_read(manager+0x14,1))==b'\1',
              'flare Init returns true/enables even when capability call reports an error')
        check(bytes(p.mu.mem_read(manager+0x24,1))[0]==int(status!=0x8876086a),
              'only D3DERR_NOTAVAILABLE disables query capability, not arbitrary failure')
    p.put_uint(f.renderer+0xc9e8,0)
    f.call(0x4c71f0,this=manager,args=(1,))
    f.close()

def main():
    lifecycle_and_name_clone();projection_dispatch();flare_capability_init()
    print(f'PASS {checks}/{checks}: PC projection/flare manager boundaries')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
