#!/usr/bin/env python3
"""Whole original manager generating miss -> template -> SDK -> device -> cache.

Prepared template and renderer device slot are explicit caller inputs; original
file/renderer construction and library/GPU implementation are not claimed.
"""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import install_char_traits,read_cstring
from pc_crt_format_fixtures import install_sprintf
from pc_shader_compiler_fixtures import ShaderCompilerOutput
from probe_pc_function_eval import cleanup

CASES={'basic':(1,0),'features':(0x0133a5a2,0x39),'many-lights':(0x00f00001,0xe4e4e4e4),'high-bits':(0xe000000f,0xffffffff),'device-failed':(1,0),'device-failed-output':(1,0)}

def main(mode,return_capture=False):
    key=CASES[mode];f=PCWriteBytesFixture();p=f.p;install_char_traits(p);install_sprintf(p)
    sdk=ShaderCompilerOutput(p,constants=(('MatDiffuse',4,1),));checks=0;maximum=0;events=[]
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    renderer=p.allocate(0xc9ec);device=p.allocate(4);table=p.allocate(0x170)
    # This consumer reads exactly renderer+C9E8, not a complete renderer object.
    p.put_uint(renderer+0xc9e8,device);p.put_uint(device,table);p.put_uint(0x75db68,renderer)
    base=0x34100000;p.mu.mem_map(base,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    hvt=p.allocate(12);handle=p.allocate(4);p.put_uint(handle,hvt);p.put_uint(hvt+8,base+16)
    def release(machine):
        check(machine.uint(machine.reg('ESP')+4)==handle,'original shader releases supplied device handle')
        events.append(['release',1]);machine.fixture_return(4,eax=0)
    p.seams[base+16]=release;p.put_uint(table+0x16c,base+32)
    def create(machine):
        d,code,output=[machine.uint(machine.reg('ESP')+4+4*i) for i in range(3)]
        check(d==device and bytes(machine.mu.mem_read(code,5))==sdk.bytecode,'original device receives just-compiled owned code')
        failure=mode.startswith('device-failed');value=0 if mode=='device-failed' else handle
        events.append(['create',sdk.bytecode.hex(),int(failure),int(bool(value))]);machine.put_uint(output,value);machine.fixture_return(12,eax=0x80004005 if failure else 0)
    p.seams[base+32]=create
    empty=p.allocate(28);call(0x450d90,this=empty);template=p.allocate(0x68);f.allocations[template]=0x68;p.mu.mem_write(template,b'\xcc'*0x68)
    call(0x4d0960,this=template,args=(1,empty,2));loader=call(0x4d54b0)
    record=loader+0x61c
    for offset,data in ((0,b'void Main(){// INSERTION POINT\n}\n'),(0x1c,b'DECL\n'),(0x38,b'Main'),(0x54,b'vs_2_0')):
        source=p.allocate(len(data)+1);p.mu.mem_write(source,data+b'\0');call(0x4cfe20,this=record+offset,args=(source,))
    p.mu.mem_write(record+0x72,b'\0');call(0x4d5fc0,this=template+0x44,args=(loader+0x548,))
    call(p.uint(p.uint(loader)),this=loader,args=(1,));call(0x59a660,this=empty)
    manager=call(0x4c9680);p.put_uint(manager+0x40,template)
    shader=call(0x4c8980,this=manager,args=key)
    check(shader!=0 and len(sdk.calls)==1,'original nonzero miss creates shader and calls compiler once')
    check(all(p.visits.get(entry) for entry in (0x4c9f10,0x4cffe0,0x4ca030,0x4c87a0)),'actual complete factory/template/device/cache insertion chain')
    check(p.uint(manager+0x4c)==1,'one actual cache node')
    second=call(0x4c8980,this=manager,args=key)
    check(second==shader and not p.visits.get(0x4c9f10) and len(sdk.calls)==1,'repeat finds same generated shader without recompilation')
    none=call(0x4c8980,this=manager,args=(key[0]&~15,key[1]))
    check(none==0 and len(sdk.calls)==1,'zero-weight bypass still precedes lookup')
    begin=p.uint(shader+0x3c);count=(p.uint(shader+0x40)-begin)//44 if begin else 0
    parameters=[[read_cstring(p,begin+i*44,32).decode(),p.uint(begin+i*44+32),p.uint(begin+i*44+36),p.uint(begin+i*44+40)] for i in range(count)]
    states=[1,int(second==shader),int(none==0),p.uint(manager+0x4c),int(bool(p.uint(shader+0x50))),p.uint(shader+0x34),parameters,bytes(p.mu.mem_read(p.uint(shader+0x4c),p.uint(shader+0x48))).hex()]
    cleanup(f,(manager,));check(set(f.allocations)==set(f.freed),'manager owns and destroys template, cached shader and generated code')
    check(set(sdk.released)=={sdk.buffer,sdk.table},'successful native compile releases all SDK results')
    capture=[mode,list(key),sdk.calls,states,events]
    if not return_capture:print('SHADER_GENERATION_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: whole original shader generating miss {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
