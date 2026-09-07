#!/usr/bin/env python3
"""Original PC4BB950/4BB9D0 scene calls and material stamp linkage.

Declared renderer storage/COM observer only, no GPU/OS/protected constructor.
Controller construction remains excluded exactly as in CP25/26.
"""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup,configure_floor,bits
from probe_pc_material_color import controller_input

def main(mode,return_capture=False):
    if mode not in ('begin','end','double-begin','double-end','begin-fail','end-fail','wrap','material'):raise ValueError('bounded scene pair')
    f=PCWriteBytesFixture();p=f.p;configure_floor(f);checks=0;events=[];states=[];maximum=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    renderer=p.allocate(0xcbc4);device=p.allocate(4);table=p.allocate(0xac)
    p.put_uint(renderer+0x18,0x6f28a0);p.put_uint(renderer+0xc9e8,device);p.put_uint(device,table);p.put_uint(0x75db68,renderer)
    p.put_uint(renderer+0x40,0xffffffff if mode=='wrap' else 0);p.put_uint(renderer+0x4c,88);p.mu.mem_write(renderer+0xcbc0,b'\x07')
    p.mu.mem_map(0x34090000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def submit(machine,kind):
        check(machine.uint(machine.reg('ESP')+4)==device,'actual COM this pointer')
        if len(events)>=4:raise AssertionError('four-call observation bound')
        events.append([kind,p.uint(renderer+0x40),p.uint(renderer+0x4c),int.from_bytes(p.mu.mem_read(renderer+0xcbc0,1),'little')])
        machine.fixture_return(4,eax=0x80004005 if mode.endswith('-fail') else 0)
    for slot,address,kind in ((0xa4,0x34090010,'begin'),(0xa8,0x34090020,'end')):
        p.put_uint(table+slot,address);p.seams[address]=lambda machine,kind=kind:submit(machine,kind)
    animations=material=controller=0
    if mode=='material':
        animations=f.call(0x454640);controller=controller_input(f);p.mu.mem_write(controller+8,b'\x01\0');material=f.call(0x4a9460)
        f.call(0x423a50,this=material,args=(controller,));p.put_uint(controller+0x1dc,8);p.put_floats(controller+0x1d0,(1,))
    sequence=['begin','end','begin','end'] if mode=='material' else ['end'] if mode=='wrap' else [mode.replace('double-','').replace('-fail','')]*(2 if mode.startswith('double-') else 1)
    for kind in sequence:
        if mode=='material' and kind=='begin':f.call(0x423190,this=controller,args=(bits(.25),))
        result=f.call(0x4bb950 if kind=='begin' else 0x4bb9d0,this=renderer+0x18)&255;maximum=max(maximum,sum(p.visits.values()))
        check(result==1,'original scene returns AL1 even if COM HRESULT fails')
        row=[kind,result,p.uint(renderer+0x40),p.uint(renderer+0x4c),int.from_bytes(p.mu.mem_read(renderer+0xcbc0,1),'little')]
        if mode=='material':
            f.call(0x4a9530,this=material+0x14,args=(0,));maximum=max(maximum,sum(p.visits.values()))
            row.extend([p.visits.get(0x4373e0,0)>0,p.uint(material+0x70),p.floats(material+0x84,1)[0],list(p.floats(controller+0x1c,2))])
        states.append(row)
    if mode=='material':
        f.call(0x423a50,this=material,args=(0,));check((p.uint(controller+8)&65535)==1,'external input pin survives no fake destructor')
        f.call(p.uint(p.uint(material)),this=material,args=(1,));f.call(0x4545d0,this=animations,args=(1,))
    cleanup(f,());check(set(f.allocations)==set(f.freed),'actual leaves/material/manager allocations freed')
    capture=[mode,events,states];print('RENDERER_SCENE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original PC scene {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
