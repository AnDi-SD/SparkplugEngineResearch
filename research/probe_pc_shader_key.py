#!/usr/bin/env python3
"""Original4BE310 key, stopping BEFORE actual manager dispatch4BE495."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup

def main(mode,return_capture=False):
    if mode not in ('components','colors','lights','uv','power','fixed'):raise ValueError('bounded shader key input')
    f=ApplyFixture();p=f.p;r=f.renderer;material=f.call(0x4a9460);p.put_uint(r+0xe47c,material)
    lights=p.allocate(0x28);light_objects=[p.allocate(0xc4) for _ in range(8)]
    for i,obj in enumerate(light_objects):p.put_uint(lights+4*i,obj)
    inputs=[];checks=0;maximum=0;captures=[]
    # input(flags, raw color mode, raw specular power, optional type vector,
    # eight raw stage-transform flags); these are consumer inputs, not defaults.
    def add(flags=0,color=4,power=0,types=None,uv=None):inputs.append([flags,color,power,types,[0]*8 if uv is None else uv])
    if mode=='components':
        for flags in [0,*[1<<i for i in range(20)],0xffffffff,0x7fffe,0x20019]:add(flags)
    elif mode=='colors':
        for color in (0,1,2,3,4,5,6,7,15,16,255,0xffffffff):add(0x180e,color)
    elif mode=='lights':
        for n in range(9):add(0x2012,types=[i%3 for i in range(n)])
        add(0x2012,types=[0xffffffff,5,7]);add(0x2012,types=None)
    elif mode=='uv':
        for mask in range(256):add(0x2012,uv=[2 if mask&(1<<i) else 0 for i in range(8)])
        for raw in (1,3,4,8,0xffffffff):add(0x2012,uv=[raw]*8)
    elif mode=='power':
        for power in (0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc00000):add(0x2012,power=power)
    else:
        for flags in (0,1,0x800,0x40801):add(flags,power=0x3f800000,types=[0,1,2],uv=[2]*8)
    for values in inputs:
        flags,color,power,types,uv=values;p.put_uint(r+0xc888,color);p.put_uint(material+0xb8,power)
        p.put_uint(r+0xc190,0 if types is None else lights);p.put_uint(lights+0x24,0 if types is None else len(types))
        for i,value in enumerate(types or ()):p.put_uint(light_objects[i]+0xc0,value)
        for stage,value in enumerate(uv):p.put_uint(r+0xc8b8+0x24*stage,value)
        p.run(0x4be310,this=r,args=(flags,),stop_at=0x4be495);maximum=max(maximum,sum(p.visits.values()))
        if p.reg('EIP')!=0x4be495 or p.visits.get(0x4c8980) or f.events:raise AssertionError('stop before dispatch, never fake shader result')
        result=[p.uint(p.reg('ESP')),p.uint(p.reg('ESP')+4)];checks+=1;captures.append([values,result])
        if mode=='fixed':
            if f.call(0x4be310,this=r,args=(flags,))!=0 or not p.visits.get(0x4c8980) or p.visits.get(0x4c89eb):raise AssertionError('actual no-weight manager NULL path')
            maximum=max(maximum,sum(p.visits.values()));checks+=1
    manager=p.uint(0x763024);cleanup(f,(material,manager));checks+=1
    capture=[mode,captures]
    if not return_capture:print('SHADER_KEY_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original shader key {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
