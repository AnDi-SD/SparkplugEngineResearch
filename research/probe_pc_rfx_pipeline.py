#!/usr/bin/env python3
"""One retained native object chain: file/regex -> XML -> compiled shader."""
from pathlib import Path
import json,sys,struct,xml.etree.ElementTree as ET
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_rfx_regex_fixtures import RFXRegexInputs
from pc_win32_file_fixtures import Win32FileInput
from pc_shader_compiler_fixtures import ShaderCompilerOutput
from pc_crt_numeric_fixtures import install_crt_numeric
from pc_stl_fixtures import read_cstring
from probe_pc_rfx_events import pass_capture
from probe_pc_function_eval import cleanup

def document(mode):
    shader=b'<RmShader PIXEL_SHADER="FALSE" CODE="x"/>'
    if mode=='hlsl':shader=b'<RmHLSLShader PIXEL_SHADER="FALSE" CODE="x" ENTRY_POINT="Main" TARGET="vs_2_0"/>'
    elif mode=='constant':shader+=b'<RmShaderConstant NAME="MatDiffuse" REGISTER="4"/>'
    elif mode!='assembly':raise ValueError('three bounded file-to-shader specimens')
    return b'<RmDirectXEffect NAME="X" TYPE=""><RmStringVariable NAME="ID" VALUE="1"/><RmPass>'+shader+b'</RmPass></RmDirectXEffect>'

def events_for(mode):
    result=[]
    def visit(node):
        result.append([True,node.tag,node.attrib])
        for child in node:visit(child)
        result.append([False,node.tag,{}])
    visit(ET.fromstring(document(mode)));return result

def main(mode,return_capture=False):
    data=document(mode);f=PCWriteBytesFixture();p=f.p;regex=RFXRegexInputs(f);install_crt_numeric(p)
    api=Win32FileInput(p,data);name=p.allocate(len(api.name)+1);p.mu.mem_write(name,api.name+b'\0');checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    loader=f.call(0x4d54b0);effect=f.call(0x4d56d0,this=loader,args=(name,));file_instructions=sum(p.visits.values())
    check(p.uint(effect+0x10)==1 and read_cstring(p,p.uint(effect+0x20))==b'X','actual file metadata input reaches template')
    check(p.uint(loader+0x540)==effect and bytes(p.mu.mem_read(effect+0x18,1))==b'\0' and not api.opened,'same owned template before XML; file closed')
    result=f.call(0x4d0650,this=effect)&255;xml_instructions=sum(p.visits.values())
    check(result==1 and bytes(p.mu.mem_read(effect+0x18,1))==b'\1','actual same template initialized')
    check(p.visits.get(0x6b52d0) and p.visits.get(0x4d4750) and p.visits.get(0x4d6050),'whole original XML library and engine callbacks')
    begin,end=p.uint(effect+0x48),p.uint(effect+0x4c)
    check(end-begin==0x1dc,'original XML produces one actual pass')
    passes=[pass_capture(p,begin)]
    sdk=ShaderCompilerOutput(p,constants=(('MatDiffuse',4,1),) if mode=='hlsl' else (),compact_storage=True)
    shader=f.call(0x4c9f10);empty=p.allocate(28);f.call(0x450d90,this=empty)
    words=struct.unpack('<7I',bytes(p.mu.mem_read(empty,28)));result=f.call(0x4cffe0,args=(begin+0xd4,shader,*words,*words))&255
    compile_instructions=sum(p.visits.values());active=int.from_bytes(p.mu.mem_read(0x73fe6b,1),'little')
    check(result==1 and active==0,'same file-produced shader record completes original compiler consumer')
    first,last=p.uint(shader+0x3c),p.uint(shader+0x40);count=(last-first)//44 if first else 0
    parameters=[[read_cstring(p,first+44*i,32).decode('ascii'),p.uint(first+44*i+32),p.uint(first+44*i+36),p.uint(first+44*i+40)] for i in range(count)]
    size=p.uint(shader+0x48);code=bytes(p.mu.mem_read(p.uint(shader+0x4c),size))
    check(code==sdk.bytecode,'entire supplied SDK output becomes owned engine bytecode')
    capture=[mode,[p.uint(effect+0x10),read_cstring(p,p.uint(effect+0x20)).decode('ascii'),data.hex()],1,passes,sdk.calls,parameters,p.uint(shader+0x34),code.hex(),active]
    regex.close();cleanup(f,(shader,loader,effect,p.uint(0x75db9c)))
    check(set(sdk.released)==({sdk.buffer,sdk.table} if mode=='hlsl' else {sdk.buffer}),'expected SDK COM results released')
    check(set(f.allocations)==set(f.freed),'all linked original engine and library allocations freed')
    if not return_capture:print('RFX_PIPELINE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original RFX pipeline {mode}; phaseInstructions={[file_instructions,xml_instructions,compile_instructions]}; initMax={max(regex.initializationInstructions)}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
