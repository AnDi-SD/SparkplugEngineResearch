#!/usr/bin/env python3
"""Actual PCShaderManager factory, byte-key cache, blank clone; no compiler/GPU."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup
def main(mode,return_capture=False):
    if mode not in ('factory','no-shader','cache','clone'):raise ValueError('bounded shader manager slice')
    f=PCWriteBytesFixture();p=f.p;checks=0;maximum=0;states=[]
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    obj=call(0x4c9680);owned=[obj]
    check(f.allocations[obj]==0x50 and p.uint(obj)==0x6f2de4,'actual PC manager50/vtable')
    check(all(p.uint(obj+i)==0 for i in (0x18,0x1c,0x20,0x28,0x2c,0x30,0x3c,0x40,0x4c)),'empty owned storage')
    for rec,identity,parent in ((0x764cb0,0xd5ae63da,0x763a80),(0x763a80,0x98ba76fe,0x755310),(0x755310,0x415352a1,0)):
        p.put_uint(rec,identity);p.put_uint(rec+0x48,parent)
    check(call(0x408370,this=obj,args=(0x98ba76fe,))&255==1,'actual registered DX manager parent')
    output=p.allocate(4);pair=p.allocate(12);tokens={0:0};inserted=[]
    def snapshot(target):
        root=p.uint(target+0x48);node=p.uint(root);result=[];iterator=output
        for _ in range(9):
            if node==root:break
            result.append([p.uint(node+0xc),p.uint(node+0x10),tokens[p.uint(node+0x14)]])
            p.put_uint(iterator,node);call(0x4c7f90,this=iterator);node=p.uint(iterator)
        else:raise AssertionError('bounded map traversal')
        check(len(result)==p.uint(target+0x4c),'exact actual map count')
        return result
    if mode in ('cache','clone'):
        # Actual PCVertexShader objects, still uncompiled. Cache hits and
        # ownership do not imply generation or usable GPU handles.
        for ordinal,key in enumerate(((0x101,0),(1,256),(257,1),(1,1),(256,0),(2,0)),1):
            shader=call(0x4c9f10);tokens[shader]=ordinal
            p.mu.mem_write(pair,struct.pack('<III',*key,shader))
            call(0x4c87a0,this=obj+0x44,args=(output,p.uint(obj+0x48),pair))
            node=p.uint(output);check(p.uint(node+0x14)==shader,'actual protected insertion and value')
            inserted.append((*key,shader))
        states.append(snapshot(obj))
        for mask,lights,shader in inserted:
            value=call(0x4c8980,this=obj,args=(mask,lights))
            check(value==(shader if mask&15 else 0),'low-weight-zero bypasses populated cache; otherwise exact hit')
            check(not p.visits.get(0x4c89eb) and not p.visits.get(0x4c9f10),'no compilation or generated object on hit')
            states.append([mask,lights,tokens[value]])
        if mode=='clone':
            call(0x52fd90,this=0x755588);clone=call(0x4c96e0,this=obj);owned.insert(0,clone)
            check(clone!=obj and p.uint(clone+0x4c)==0 and p.uint(clone+0x40)==0,'actual clone is blank, not a cache copy')
            states.append(snapshot(clone));call(0x6d7db0)
        for key in ((1,2),(0x10001,0),(3,0)):
            p.mu.mem_write(pair,struct.pack('<II',*key));call(0x4c80f0,this=obj+0x44,args=(output,pair))
            check(p.uint(output)==p.uint(obj+0x48),'original find miss returns sentinel')
            states.append([*key,False])
    if mode in ('factory','no-shader'):
        for key in ((0,0),(0x12345670,0xabcdef12),(0xfffffff0,0xffffffff)):
            result=call(0x4c8980,this=obj,args=key)
            check(result==0 and not p.visits.get(0x4c80f0),'zero blend-weight nibble returns NULL without lookup')
            states.append([*key,0])
        states.append(snapshot(obj))
    p.put_uint(0x763024,0x12345678);cleanup(f,owned)
    check(p.uint(0x763024)==0 and set(f.allocations)==set(f.freed),'all actual shaders/maps owned and freed; singleton cleared')
    capture=[mode,states]
    if not return_capture:print('SHADER_MANAGER_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original shader manager {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
