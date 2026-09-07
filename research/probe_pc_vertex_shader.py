#!/usr/bin/env python3
"""PC vertex shader factory/copy/create/lifetime, external COM only."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup

def main(mode,return_capture=False):
    if mode not in ('factory','clone','create','failed','failed-output','repeat'):raise ValueError('bounded vertex shader slice')
    f=ApplyFixture();p=f.p;obj=f.call(0x4c9f10);owned=[obj];events=[];checks=0;maximum=sum(p.visits.values());tokens={0:0}
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    check(f.allocations[obj]==0x54 and p.uint(obj)==0x6f2ecc,'actual PC vertex shader54')
    check(all(p.uint(obj+i)==0 for i in (*range(0x10,0x38,4),0x3c,0x40,0x44,0x48,0x4c,0x50)) and p.uint(obj+0x38)==0xcccccccc,'actual defaults/untouched allocator')
    for record,identity,parent in ((0x764dd0,0x59d92171,0x765758),(0x765758,0x7a6743a9,0x763a20),(0x763a20,0x468a0ac1,0x75ac00),(0x75ac00,0x20a72504,0x7555f8),(0x7555f8,0x44de07fd,0x755310),(0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    check(call(0x408370,this=obj,args=(0x468a0ac1,))&255==1,'actual shader ancestry')
    captures=[]
    def snapshot(target):return [[p.uint(target+i) for i in range(0x14,0x38,4)],tokens[p.uint(target+0x50)]]
    captures.append(snapshot(obj))
    if mode=='clone':
        p.put_uint(obj+0x14,0x12345678);p.put_uint(obj+0x34,7);captures.append(snapshot(obj))
        call(0x52fd90,this=0x755588);clone=call(0x4c9f80,this=obj);owned.insert(0,clone)
        check(clone and p.uint(clone+0x14)==0 and p.uint(clone+0x34)==0 and p.visits.get(0x413120),'clone copies name only, no shader fields')
        captures.append(snapshot(clone));call(0x6d7db0)
    elif mode not in ('factory',):
        code=p.allocate(16);words=[0xfffe0101,1,0x80000000,0xffff]
        for i,value in enumerate(words):p.put_uint(code+4*i,value)
        table=p.allocate(12);handles=[]
        for ordinal in (1,2):
            handle=p.allocate(4);p.put_uint(handle,table);tokens[handle]=ordinal;handles.append(handle)
        p.put_uint(table+8,0x34090e40)
        def release(machine):
            value=machine.uint(machine.reg('ESP')+4);events.append(['release',tokens[value]])
            machine.fixture_return(4,eax=0)
        p.seams[0x34090e40]=release;p.put_uint(p.uint(f.device)+0x16c,0x34090e50);ordinal=0
        def create(machine):
            nonlocal ordinal
            device,bytecode,output=[machine.uint(machine.reg('ESP')+4+4*i) for i in range(3)]
            check(device==f.device and bytecode==code and output==obj+0x50,'exact actual COM device/code/output field')
            before=tokens[machine.uint(output)];failure=mode.startswith('failed');new=0 if mode=='failed' else handles[ordinal]
            machine.put_uint(output,new);ordinal+=1
            events.append(['create',words,before,tokens[new],int(failure)])
            machine.fixture_return(12,eax=0x80004005 if failure else 0)
        p.seams[0x34090e50]=create
        for unused in ((0,0xffffffff) if mode=='repeat' else (0x12345678,)):
            result=call(0x4ca030,this=obj,args=(code,unused))&255
            check(result==1,'actual creation ignores HRESULT and unused arg2');captures.append([unused,result,snapshot(obj)])
        check(not any(event[0]=='release' for event in events),'create does not release overwritten handle')
    cleanup(f,owned);check(set(f.allocations)==set(f.freed),'all native shader allocations freed')
    capture=[mode,captures,events]
    if not return_capture:print('VERTEX_SHADER_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original vertex shader {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
