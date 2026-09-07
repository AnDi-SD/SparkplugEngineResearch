#!/usr/bin/env python3
"""Original engine RFX callbacks with decoded XML-library input, CP53."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import install_char_traits,read_cstring
from pc_xml_event_fixtures import DecodedXmlAttributes
from probe_pc_function_eval import cleanup

def start(tag,**attributes):return [True,tag,attributes]
def end(tag):return [False,tag,{}]

CASES={
 'parameter-vertex':[start('RmShader',PIXEL_SHADER='FALSE',CODE='vs.1.1'),start('RmShaderConstant',NAME='MatDiffuse',REGISTER='17'),end('RmShader'),end('RmPass')],
 'parameter-pixel':[start('RmShader',PIXEL_SHADER='TRUE'),start('RmShaderConstant',NAME='ConstColor',REGISTER=' +7tail'),start('RmShaderConstant',NAME='unknown',REGISTER='-1'),start('RmShaderConstant',NAME='',REGISTER='invalid'),end('RmPass')],
 'parameter-missing':[start('RmShader',PIXEL_SHADER='FALSE'),start('RmShaderConstant',REGISTER='9'),start('RmShaderConstant',NAME='AmbientCol'),start('RmShaderConstant',NAME='AmbientCol',REGISTER=''),end('RmPass')],
 'parameter-limits':[start('RmShader',PIXEL_SHADER='FALSE'),start('RmShaderConstant',NAME='x'*31,REGISTER='-2147483648'),start('RmShaderConstant',NAME='MatDiffuse',REGISTER='2147483647'),end('RmPass')],
 'parameter-persist':[start('RmShader',PIXEL_SHADER='FALSE'),start('RmShaderConstant',NAME='MatDiffuse',REGISTER='1'),end('RmPass'),start('RmShader',PIXEL_SHADER='TRUE'),start('RmShaderConstant',NAME='AmbientCol',REGISTER='3'),end('RmPass')],
 'shader-preserve':[start('RmHLSLShader',PIXEL_SHADER='FALSE',CODE='first',DECLARATION_BLOCK='decl',ENTRY_POINT='Main',TARGET='vs_2_0'),start('RmHLSLShader',PIXEL_SHADER='FALSE',CODE='second',TARGET='vs_3_0'),start('RmHLSLShader',PIXEL_SHADER='FALSE'),start('RmShader',PIXEL_SHADER='FALSE',CODE='asm'),end('RmPass')],
 'stream-usage':[start('RmStreamChannel'),start('RmStreamChannel',USAGE='06'),start('RmStreamChannel',USAGE='6'),start('RmStreamChannel',USAGE='0')],
 'ignored-nodes':[start('RmDirectXEffect'),start('RmStringVariable',NAME='ID',VALUE='ABC'),start('RmMatrixVariable'),start('RmState',NAME='ZEnable',VALUE='FALSE'),start('RmTextureReference'),start('Unknown'),end('Unknown'),end('RmPass')],
}

def shader_capture(p,shader):
    strings=[]
    for field in (0,0x1c,0x38,0x54):
        slot=shader+field;size=p.uint(slot+0x14);capacity=p.uint(slot+0x18)
        if size>capacity or size>0x8000:raise AssertionError('bounded original MSVC shader string size')
        data=p.uint(slot+4) if capacity>=16 else slot+4
        if bytes(p.mu.mem_read(data+size,1))!=b'\0':raise AssertionError('original MSVC shader string terminator')
        strings.append(bytes(p.mu.mem_read(data,size)).decode('latin1'))
    begin,end=p.uint(shader+0x78),p.uint(shader+0x7c)
    count=(end-begin)//44 if begin else 0
    if not 0<=count<=8 or end-begin!=count*44:raise AssertionError('bounded actual parameter vector')
    parameters=[[read_cstring(p,begin+44*i,32).decode('latin1'),p.uint(begin+44*i+0x24),p.uint(begin+44*i+0x28)] for i in range(count)]
    # Descriptor type at+20 is not initialized in the callback's stack record.
    return [strings,list(bytes(p.mu.mem_read(shader+0x70,3))),parameters]

def pass_capture(p,record):return [shader_capture(p,record+offset) for offset in (0xd4,0x158)]

def main(mode,return_capture=False):
    if mode not in CASES:raise ValueError('bounded RFX callback mode')
    f=PCWriteBytesFixture();p=f.p;install_char_traits(p);xml=DecodedXmlAttributes(p);checks=0;maximum=0
    def call(entry,this=0,args=(),cdecl=False):
        nonlocal maximum
        if cdecl:
            p.run(entry,this=this,args=args,callee_pop=False)
            if p.uint(f.teb)!=0xffffffff:raise AssertionError('original callback restores SEH')
            result=p.reg('EAX')
        else:result=f.call(entry,this=this,args=args)
        maximum=max(maximum,sum(p.visits.values()));return result
    string=p.allocate(28);call(0x450d90,this=string)
    obj=p.allocate(0x68);f.allocations[obj]=0x68;p.mu.mem_write(obj,b'\xcc'*0x68)
    call(0x4d0960,this=obj,args=(1,string,2));p.put_uint(obj+0x40,0xa5a50020)
    loader=call(0x4d54b0);p.put_uint(loader+0x540,obj);p.put_uint(loader+0x544,xml.handle)
    call(0x4cffc0,this=loader+0x548);captures=[]
    for is_start,tag,attributes in CASES[mode]:
        xml.set(attributes);name=xml.string(tag)
        if is_start:call(0x4d4750,args=(loader,0,0,name,0),cdecl=True)
        else:call(0x4d6050,args=(loader,0,0,name),cdecl=True)
        checks+=1
        if is_start and not p.visits.get(0x4d3a80):raise AssertionError('actual start callback/dispatch')
        begin,end=p.uint(obj+0x48),p.uint(obj+0x4c);count=(end-begin)//0x1dc if begin else 0
        if not 0<=count<=2:raise AssertionError('bounded pass vector')
        active=p.uint(loader+0x724)
        if active not in (0,loader+0x61c,loader+0x6a0):raise AssertionError('unknown shader pointer')
        captures.append([p.uint(obj+0x40),-1 if not active else int(active==loader+0x6a0),pass_capture(p,loader+0x548),[pass_capture(p,begin+i*0x1dc) for i in range(count)]])
    call(0x59a660,this=string);cleanup(f,(loader,obj));checks+=1
    if set(f.allocations)!=set(f.freed):raise AssertionError('all original callback allocations freed')
    capture=[mode,captures]
    if not return_capture:print('RFX_EVENTS_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original RFX events {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
