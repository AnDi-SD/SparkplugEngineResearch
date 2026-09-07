#!/usr/bin/env python3
"""Original PC effect-template construction/lifetime and type dispatch."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import install_char_traits,read_cstring
from probe_pc_function_eval import cleanup
from pc_crt_memory_fixtures import install_crt_memory
from pc_crt_numeric_fixtures import install_crt_numeric

XML_CASES={
    'xml-empty':b'<Root/>',
    'xml-pass':b'<Root><RmPass NAME="One" PASS_INDEX="3" ENABLED="FALSE"/></Root>',
    'xml-hlsl':b'<Root><RmPass><RmHLSLShader CODE="a&amp;b&#xA;c" DECLARATION_BLOCK="decl" TARGET="vs_1_1" ENTRY_POINT="Main" PIXEL_SHADER="FALSE"/></RmPass></Root>',
    'xml-vs20':b'<RmPass><RmHLSLShader PIXEL_SHADER="FALSE" TARGET="vs_2_0"/></RmPass>',
    'xml-ps11':b'<RmPass><RmHLSLShader PIXEL_SHADER="TRUE" TARGET="ps_1_1"/></RmPass>',
    'xml-ps14':b'<RmPass><RmHLSLShader PIXEL_SHADER="TRUE" TARGET="ps_1_4"/></RmPass>',
    'xml-ps20':b'<RmPass><RmHLSLShader PIXEL_SHADER="TRUE" TARGET="ps_2_0"/></RmPass>',
    'xml-unknown-target':b'<RmPass><RmHLSLShader PIXEL_SHADER="true" TARGET="vs_3_0" CODE="x"/></RmPass>',
    'xml-missing-pixel':b'<RmPass><RmHLSLShader TARGET="vs_2_0" CODE="x"/></RmPass>',
    'xml-assembly':b'<RmPass><RmShader PIXEL_SHADER="TRUE" CODE="ps.1.1"/></RmPass>',
    'xml-ignored':b'<RmPass><RmState NAME="ZEnable" VALUE="FALSE"/><RmTextureReference/></RmPass>',
    'xml-constant':b'<RmPass><RmShader PIXEL_SHADER="FALSE"/><RmShaderConstant NAME="MatDiffuse" REGISTER="3"/></RmPass>',
}

def main(mode,return_capture=False):
    if mode not in ('empty','text','long-text','kind0','kind1','kind3','owned-text',*XML_CASES):raise ValueError('bounded template slice')
    f=PCWriteBytesFixture();p=f.p;install_char_traits(p);checks=0;maximum=0
    if mode in XML_CASES:install_crt_memory(f)
    if mode=='xml-constant':install_crt_numeric(p)
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    data=XML_CASES[mode] if mode in XML_CASES else b'' if mode=='empty' else b'A longer native template source' if mode=='long-text' else b'Fixture'
    chars=p.allocate(len(data)+1);p.mu.mem_write(chars,data+b'\0')
    string=p.allocate(0x1c);call(0x450d90,this=string)
    call(0x40fd50,this=string,args=(chars,len(data)))
    obj=p.allocate(0x68);f.allocations[obj]=0x68;p.mu.mem_write(obj,b'\xcc'*0x68)
    kind=0 if mode=='kind0' else 1 if mode=='kind1' else 3 if mode=='kind3' else 2
    check(call(0x4d0960,this=obj,args=(0x12345678,string,kind))==obj,'actual template constructor')
    check(p.uint(obj)==0x6f3a64 and p.uint(obj+0x10)==0x12345678 and p.uint(obj+0x1c)==kind,'identity/kind/vtable')
    capacity=p.uint(obj+0x3c);stored=read_cstring(p,p.uint(obj+0x28) if capacity>=16 else obj+0x28)
    check(stored==data and p.uint(obj+0x38)==len(data),'owned exact MSVC source string copy')
    check(all(p.uint(obj+i)==0 for i in (0x14,0x20,0x48,0x4c,0x50,0x58,0x5c,0x60,0x64)),'empty template ownership vectors')
    check(p.uint(obj+0x18)==0xcccccc00 and p.uint(obj+0x40)==0xcccccccc and p.uint(obj+0x44)==0xcccccccc and p.uint(obj+0x54)==0xcccccccc,'untouched native fields remain unknown')
    states=[]
    if mode in ('kind0','kind1','kind3',*XML_CASES):
        for _ in range(2):
            result=call(0x4d0650,this=obj)&255
            check(result==1 and bytes(p.mu.mem_read(obj+0x18,1))==b'\1','successful dispatch marks template ready')
            if mode in ('kind0','kind3'):check(not p.visits.get(0x4d6090) and not p.visits.get(0x4d74a0),'actual fallback does not parse')
            elif mode=='kind1':check(p.visits.get(0x4d74a0),'kind1 actual stub returns success')
            states.append(result)
            if mode in XML_CASES:
                begin,end=p.uint(obj+0x48),p.uint(obj+0x4c)
                count=(end-begin)//0x1dc if begin else 0
                check(count<=2,'bounded XML pass capture')
                passes=[]
                for i in range(count):
                    record=begin+i*0x1dc;shaders=[]
                    for offset in (0xd4,0x158):
                        shader=record+offset;strings=[]
                        for field in (0,0x1c,0x38,0x54):
                            slot=shader+field
                            strings.append(read_cstring(p,p.uint(slot+4) if p.uint(slot+0x18)>=16 else slot+4).decode('latin1'))
                        shaders.append([strings,list(bytes(p.mu.mem_read(shader+0x70,3)))])
                    if mode=='xml-constant':
                        from probe_pc_rfx_events import pass_capture
                        shaders=pass_capture(p,record)
                    passes.append(shaders)
                states.append(passes)
    if mode=='owned-text':
        # Input is an explicit already-owned field at the loader boundary.
        # Template destruction owns both14 and20 independently.
        for offset,owned_data in ((0x14,b'field14\0'),(0x20,b'name\0')):
            address=p.allocate(len(owned_data));f.allocations[address]=len(owned_data);p.mu.mem_write(address,owned_data);p.put_uint(obj+offset,address)
    call(0x59a660,this=string);cleanup(f,(obj,))
    check(set(f.allocations)==set(f.freed),'all actual string/template owned allocations freed')
    capture=[mode,0x12345678,kind,data.decode('ascii'),states]
    if not return_capture:print('EFFECT_TEMPLATE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original effect template {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
