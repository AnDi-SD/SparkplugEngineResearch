#!/usr/bin/env python3
"""Original4AE930 parameter dispatch with bounded declared consumer inputs."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup,bits
from pc_serializer_fixtures import PCWriteBytesFixture

def names(return_capture=False):
    f=PCWriteBytesFixture();p=f.p;buffer=p.allocate(96);capture=['names',[]];maximum=0
    for index in range(23):
        address=p.uint(0x6efae0+4*index);value=bytearray()
        for i in range(80):
            byte=p.mu.mem_read(address+i,1)[0]
            if not byte:break
            value.append(byte)
        else:raise AssertionError('bounded original name table')
        p.mu.mem_write(buffer,bytes(value)+b'\0');p.run(0x4ae660,args=(buffer,),callee_pop=False)
        maximum=max(maximum,sum(p.visits.values()));capture[1].append([value.decode('ascii'),p.reg('EAX')])
    for value in ('MatDiffuseX','matdiffuse','LightPos[0]','unknown'):
        p.mu.mem_write(buffer,value.encode()+b'\0');p.run(0x4ae660,args=(buffer,),callee_pop=False)
        maximum=max(maximum,sum(p.visits.values()));capture[1].append([value,p.reg('EAX')])
    cleanup(f,())
    if not return_capture:print('SHADER_CONSTANTS_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS 28/28: original shader semantic names; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

def main(mode,return_capture=False):
    if mode=='names':return names(return_capture)
    if mode not in ('matrices','material','unknown','blend','uv','light-colors','light-missing','inverse','light-geometry','geometry-missing'):raise ValueError('bounded known descriptor families')
    f=ApplyFixture(renderer_size=0xf358);p=f.p;r=f.renderer;obj=f.call(0x4c9f10);material=f.call(0x4a9460);p.put_uint(r+0xe47c,material)
    p.run(0x412400,args=(44,),callee_pop=False);descriptor=p.reg('EAX')
    p.put_uint(obj+0x3c,descriptor);p.put_uint(obj+0x40,descriptor+44);p.put_uint(obj+0x44,descriptor+44)
    p.mu.mem_write(descriptor,bytes(44));output=p.allocate(512);checks=0;maximum=0;captures=[]
    # Cached matrix getter dirty byteF2F4 remains explicitly0. Full matrix
    # recomputation4AD540 is not silently replaced by these cached inputs.
    p.mu.mem_write(r+0xf2f4,b'\0')
    for ordinal,offset in enumerate((0xcb00,0xcb40,0xcb80),1):
        for i in range(16):p.put_uint(r+offset+4*i,bits(float(ordinal*10+i)))
    for offset,values in ((0x78,(.25,.5,.75,1.)),(0x88,(.125,.375,.625,.875)),(0x98,(1.5,2.,2.5,3.)),(0xa8,(.2,.4,.6,.8))):
        for i,value in enumerate(values):p.put_uint(material+offset+4*i,bits(value))
    p.put_uint(material+0xb8,bits(7.5));p.put_uint(r+0xc194,0x7f234567)
    if mode=='inverse':
        view=(2.,0.,0.,0.,0.,4.,0.,0.,0.,0.,.5,0.,3.,5.,7.,1.)
        for i,value in enumerate(view):p.put_uint(r+0xcb00+4*i,bits(value))
        inputs=[(4,start,count) for start,count in ((0,4),(3,2),(2,0))]
    elif mode=='uv':
        for stage in range(2):
            for i in range(16):p.put_uint(r+0xf0f4+64*stage+4*i,bits(float(200+stage*16+i)))
        inputs=[(17,start,count) for start in (0,3) for count in range(7)]
    elif mode in ('light-colors','light-missing','light-geometry','geometry-missing'):
        lights=p.allocate(0x28);p.put_uint(r+0xc190,lights);p.put_uint(lights+0x24,2)
        light_objects=[p.allocate(0x158) for _ in range(2)]
        for i,light in enumerate(light_objects):
            p.put_uint(lights+4*i,light)
            p.put_uint(light+0xc0,i)
            for j,value in enumerate((.25+i,.5+i,.75+i,1.+i)):p.put_uint(light+0xc4+4*j,bits(value))
            for j,offset in enumerate((0x144,0x148,0x14c,0xe0)):p.put_uint(light+offset,bits(float(i*10+j+1)))
            for offset,values in ((0x58,(.25+i,.5+i,.75+i)),(0x74,(4.+i,5.+i,6.+i)),(0xa4,(1.+i,2.+i,3.+i))):
                for j,value in enumerate(values):p.put_uint(light+offset+4*j,bits(value))
            p.put_uint(light+0xe4,bits(.5+i));p.put_uint(light+0xe8,bits(1.+i))
        if mode in ('light-colors','light-geometry'):p.put_uint(lights+0x20,light_objects[1]);p.put_uint(r+0xf2f8,light_objects[0])
        for i,value in enumerate((.25,.5,.75,1.)):p.put_uint(r+0xc178+4*i,bits(value))
        geometry=mode in ('light-geometry','geometry-missing')
        if geometry:
            for i,value in enumerate((1.,2.,3.,0.,4.,5.,6.,0.,7.,8.,10.,0.,10.,11.,12.,1.)):p.put_uint(r+0xca80+4*i,bits(value))
        inputs=[(kind,start,count) for kind in ((13,14,15,16,18) if geometry else (6,11,12,19,20,21,22)) for start,count in ((0,1),(3,2),(2,0))]
    elif mode=='blend':
        matrices=p.allocate(128);p.put_uint(r+0xc9b8,matrices)
        for i in range(32):p.put_uint(matrices+4*i,bits(float(100+i)))
        inputs=[(5,start,count) for start in (0,3) for count in range(7)]
    else:inputs=[(kind,start,count) for kind in ((1,2,3) if mode=='matrices' else (7,8,9,10) if mode=='material' else (0,23,0xffffffff)) for start,count in ((0,1),(3,2),(2,0))]
    for kind,start,count in inputs:
        for offset,value in ((0x20,kind),(0x24,start),(0x28,count)):p.put_uint(descriptor+offset,value)
        p.mu.mem_write(output,b'\xa5'*512);f.call(0x4ae930,this=obj,args=(output,));maximum=max(maximum,sum(p.visits.values()));checks+=1
        if f.events or p.visits.get(0x4ad540):raise AssertionError('no COM or fake dirty-matrix recomputation')
        captures.append([kind,start,count,[p.uint(output+4*i) for i in range(64)]])
    cleanup(f,(obj,material));checks+=1;capture=[mode,captures]
    if not return_capture:print('SHADER_CONSTANTS_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original shader constants {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
