#!/usr/bin/env python3
"""Original template source specialization, SDK result consumer and cleanup."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import install_char_traits,read_cstring
from pc_shader_compiler_fixtures import ShaderCompilerOutput
from probe_pc_function_eval import cleanup

CASES={
 'assembly':dict(code='CODE',decl='DECL',header='HEAD',insert='INS',hlsl=False),
 'insert':dict(code='A// INSERTION POINT\nB',decl='DECL\n',header='HEAD\n',insert='INS\n',hlsl=False),
 'hlsl':dict(code='CODE',decl='DECL',header='HEAD',insert='INS',hlsl=True),
}
for mode,changes in {
 'two-markers':dict(code='A// INSERTION POINT B// INSERTION POINT'),
 'marker-header':dict(header='// INSERTION POINT\nHEAD'),
 'marker-declarations':dict(decl='// INSERTION POINT\nDECL'),
 'empty-insert':dict(code='// INSERTION POINT',insert=''),
 'empty-source':dict(code='',decl='',header=''),
 'assembly-parameters':dict(parameters=[('MatDiffuse',4,1),('unknown',7,1)]),
 'assembly-hresult':dict(hresult=0x80004005),
 'assembly-warning':dict(warning=True),
 'hlsl-empty-reflection':dict(hlsl=True,constants=[]),
 'hlsl-hresult':dict(hlsl=True,hresult=0x80004005),
 'hlsl-warning':dict(hlsl=True,warning=True),
 'hlsl-unknown':dict(hlsl=True,constants=[('TextureMap',3,1),('MatDiffuse',9,0)]),
}.items():CASES[mode]={**CASES['assembly'],**changes}

def main(mode,return_capture=False):
    case=CASES[mode];f=PCWriteBytesFixture();p=f.p;install_char_traits(p)
    sdk=ShaderCompilerOutput(p,constants=case.get('constants',(('MatDiffuse',4,1),('view_proj_matrix',8,4))),hresult=case.get('hresult',0),warning=case.get('warning',False))
    maximum=0;checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    def assign(string,data):
        raw=data.encode()+b'\0';address=p.allocate(len(raw));p.mu.mem_write(address,raw);call(0x4cfe20,this=string,args=(address,))
    def value_string(data):
        string=p.allocate(28);call(0x450d90,this=string);assign(string,data)
        # Explicit value-argument ownership transfers to original stack copy;
        # do not destroy this synthetic duplicate of its seven words again.
        return struct.unpack('<7I',bytes(p.mu.mem_read(string,28)))
    loader=call(0x4d54b0);record=loader+0x61c;shader=call(0x4c9f10)
    for offset,value in ((0,case['code']),(0x1c,case['decl']),(0x38,'Main'),(0x54,'vs_2_0')):assign(record+offset,value)
    p.mu.mem_write(record+0x72,bytes([not case['hlsl']]))
    for name,reg,count in case.get('parameters',[]):
        descriptor=p.allocate(44);encoded=name.encode()+b'\0';p.mu.mem_write(descriptor,encoded+b'\xcc'*(32-len(encoded)))
        p.put_uint(descriptor+32,0xdeadbeef);p.put_uint(descriptor+36,reg);p.put_uint(descriptor+40,count)
        call(0x4af850,this=record+0x74,args=(descriptor,))
    args=(record,shader,*value_string(case['insert']),*value_string(case['header']))
    result=call(0x4cffe0,args=args)&255
    check(result==1,'actual compiler consumer returns success with supplied code buffer, regardless HRESULT')
    check(bytes(p.mu.mem_read(0x73fe6b,1))==b'\0','successful consumer clears compile activity flag')
    begin,end=p.uint(shader+0x3c),p.uint(shader+0x40);count=(end-begin)//44 if begin else 0
    descriptors=[[read_cstring(p,begin+44*i,32).decode('latin1'),p.uint(begin+44*i+32),p.uint(begin+44*i+36),p.uint(begin+44*i+40)] for i in range(count)]
    size=p.uint(shader+0x48);bytecode=bytes(p.mu.mem_read(p.uint(shader+0x4c),size)).hex()
    capture=[mode,result,sdk.calls,sdk.events,descriptors,p.uint(shader+0x34),bytecode,list(bytes(p.mu.mem_read(0x73fe6b,1)))]
    check(bytecode==sdk.bytecode.hex(),'all bytecode bytes copied including nonword tail')
    check(len(sdk.calls)==1 and sdk.calls[0][-1]==[1],'single selected SDK call with activity flag set')
    cleanup(f,(shader,loader))
    expected={sdk.buffer,sdk.table} if case['hlsl'] else {sdk.buffer}
    if sdk.warning:expected.add(sdk.warning)
    check(set(sdk.released)==expected,'all expected compiler objects released')
    check(set(f.allocations)==set(f.freed),'all engine-owned strings, descriptors, code and objects freed')
    if not return_capture:print('SHADER_COMPILE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original template compile {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
