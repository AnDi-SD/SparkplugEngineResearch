#!/usr/bin/env python3
"""Original Skin clone/copy using real map-aware and always-clone bodies."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_relationships import node_rtti
from pc_stl_fixtures import install_char_traits
from probe_pc_function_eval import cleanup

MODES=('empty','one','mapped','repeat','direct-repeat','child','copy-populated','raw-bits')

def main(mode,return_capture=False):
    if mode not in MODES:raise ValueError('bounded original clone specimen')
    f=PCWriteBytesFixture();p=f.p;node_rtti(f);install_char_traits(p)
    f.call(0x6d38e0);f.call(0x52fd90,this=0x755588);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def allocate(n):p.run(0x417190,args=(n,),callee_pop=False);return p.reg('EAX')
    for record,identity,parent in ((0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030),(0x760520,0x681f2043,0x760cf8)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    source=f.call(0x46a120);manager=f.call(0x412540);p.put_uint(0x74e060,manager);bone=f.call(0x421e20)
    p.put_floats(bone+0x20,(1,2,3));p.put_floats(bone+0x74,(1,2,3))
    count=0 if mode=='empty' else (2 if mode in ('repeat','direct-repeat','child') else 1)
    child=0
    if mode=='child':
        child=f.call(0x421e20);p.put_floats(child+0x20,(4,5,6));f.call(0x421a60,this=bone,args=(child,))
    matrices=b''
    for i in range(count):
        matrices+=struct.pack('<16I',0x80000000,0x7fc12345,0x7f800000,0xff800000,*range(12)) if mode=='raw-bits' else struct.pack('<16f',*(float(j+i) for j in range(16)))
    if count:
        pointers=allocate(count*4);storage=allocate(count*64)
        for i in range(count):p.put_uint(pointers+i*4,child if mode=='child' and i==1 else bone)
        p.mu.mem_write(storage,matrices);f.call(0x46a100,this=source,args=(count,pointers,storage))
    p.put_uint(source+0x60,3);p.put_uint(source+0x5c,17)
    mapped=0;old=[]
    if mode=='mapped':
        mapped=f.call(0x421e20);p.put_floats(mapped+0x20,(7,8,9));f.call(0x412f70,this=manager,args=(bone,mapped))
    if mode=='copy-populated':
        clone=f.call(0x46a120);old=[allocate(4),allocate(64)];p.put_uint(old[0],bone);p.mu.mem_write(old[1],bytes(64))
        f.call(0x46a100,this=clone,args=(1,*old))
        result=f.call(0x46a650,this=source,args=(clone,))&255
    elif mode=='direct-repeat':clone=f.call(0x46a1a0,this=source);result=int(bool(clone))
    else:clone=f.call(0x412be0,this=manager,args=(source,));result=int(bool(clone))
    instructions=sum(p.visits.values())
    check(result==1 and clone!=source and f.allocations[clone]==0x70,'whole Skin copy/clone succeeds with actual allocation')
    check(p.visits.get(0x46a650) and p.visits.get(0x479e60),'actual Skin and Model copy bodies')
    if count:check(p.visits.get(0x412c40),'actual map-aware bone resolver, never pair/lookup seam')
    check(p.uint(clone+0x60)==3 and p.uint(clone+0x64)==count and p.uint(clone+0x5c)==17,'weights/count/inherited projection group copied')
    actual=bytes(p.mu.mem_read(p.uint(clone+0x6c),count*64)) if count else b''
    check(actual==matrices,'all matrix bits copied into independent owned storage')
    if count:check(p.uint(clone+0x68)!=p.uint(source+0x68) and p.uint(clone+0x6c)!=p.uint(source+0x6c),'both destination arrays independent')
    check(all(a in f.freed for a in old),'preexisting destination arrays freed before replacement')
    targets=[p.uint(p.uint(clone+0x68)+4*i) for i in range(count)]
    if targets:
        check(all(a not in (bone,child) for a in targets),'unmapped source bones are cloned, not retained')
        if mode=='mapped':check(targets==[mapped],'existing map hit returns exact supplied borrowed Node')
        if mode=='repeat':check(targets[0]==targets[1],'outer root transaction preserves one repeated bone')
        if mode=='direct-repeat':check(targets[0]!=targets[1],'direct clone lets each bone miss complete a separate root transaction')
        if mode=='child':check(p.uint(targets[1]+0x2c)==targets[0] and p.uint(targets[0]+0x1c)==1,'second Skin bone maps to already cloned Node child')
    check(p.uint(manager+0x14)==0 and p.uint(0x755590)==0,'actual root completion clears static mapping and returns depth zero')
    unique=list(dict.fromkeys(targets));nodes=[]
    for obj in targets:
        parent=p.uint(obj+0x2c)
        nodes.append([unique.index(obj),int(obj==mapped),unique.index(parent) if parent in unique else -1,
            bytes(p.mu.mem_read(obj+0x20,12)).hex(),bytes(p.mu.mem_read(obj+0x74,12)).hex(),p.uint(obj+0xb0)])
    capture=[mode,result,p.uint(clone+0x60),count,p.uint(clone+0x5c),actual.hex(),nodes]
    f.call(0x6d7db0)
    # Skin arrays borrow all bone pointers. Parents separately own children.
    owners=[source,clone,bone]+[n for n in unique if not p.uint(n+0x2c)]
    if mapped and mapped not in owners:owners.append(mapped)
    owners+=list(p.uint(a) for a in (0x75db90,0x75db78) if p.uint(a))
    cleanup(f,tuple(dict.fromkeys(owners)))
    check(set(f.allocations)==set(f.freed),'all arrays, nodes, static map and manager allocations freed')
    if not return_capture:print('SKIN_CLONE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original Skin clone {mode}; instructions={instructions}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
