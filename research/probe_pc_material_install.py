#!/usr/bin/env python3
"""Original PC material installation/state batch, no renderer ctor/GPU."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_material_color import controller_input,OFFSETS
from probe_pc_function_eval import cleanup,configure_floor,bits

class InstallFixture(ApplyFixture):
    def observe(self,p,name,argc):
        super().observe(p,name,argc)
        if name!='render':raise AssertionError('unexpected material batch device call')
        self.events[-1].extend([[p.uint(self.renderer+0xc868+4*i) for i in range(11)],
            [p.uint(self.renderer+0xe4a4+4*i) for i in range(17)]])

def main(mode,return_capture=False):
    if mode not in ('install-none','install-color','install-shared','batch-default','batch-override','batch-failed'):raise ValueError('bounded material installation case')
    f=InstallFixture(mode=='batch-failed');p=f.p;r=f.renderer;configure_floor(f)
    material=f.call(0x4a9460);materials=[material];checks=0;maximum=0;captures=[]
    p.put_floats(material+0xb8,(8.,));p.put_uint(r+0xc194,0x80402010)
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def words(address,n):return [p.uint(address+4*i) for i in range(n)]
    if mode.startswith('install-'):
        animations=f.call(0x454640);controller=controller_input(f);p.mu.mem_write(controller+8,b'\x01\0')
        if mode!='install-none':f.call(0x423a50,this=material,args=(controller,))
        if mode=='install-shared':
            materials.append(f.call(0x4a9460));p.put_floats(materials[-1]+0xb8,(16.,));f.call(0x423a50,this=materials[-1],args=(controller,))
        for i,offset in enumerate(OFFSETS):
            p.put_uint(controller+offset+0x10,0x80402010+i);p.put_uint(controller+offset+0x14,0xff112244+i)
            p.put_uint(controller+offset+0x4c,7);p.put_floats(controller+offset+0x3c,(i*.25-.5,))
        p.put_uint(controller+0x1dc,8);p.put_floats(controller+0x1cc,(0.,));p.put_floats(controller+0x1d0,(1.,))
        for frame,delta in ((0,.25),(0,.5),(1,.25),(1,0),(2,.5),(3,0)):
            p.put_uint(r+0x40,frame);f.call(0x423190,this=controller,args=(bits(delta),))
            for index,owner in enumerate(materials):
                before=words(owner+0x78,17);f.events.clear();result=f.call(0x4be180,this=r,args=(owner,))&255
                maximum=max(maximum,sum(p.visits.values()));evaluated=p.visits.get(0x4373e0,0)>0
                check(result==1 and p.uint(r+0xe47c)==owner and not f.events,'installation borrows material, no device command')
                check(words(r+0xe4a4,17)==before,'native copies material colors BEFORE frame/controller update')
                captures.append([frame,delta,index,result,evaluated,before,words(r+0xe4a4,17),[words(m+0x78,17) for m in materials],[p.uint(m+0x70) for m in materials],words(controller+0x1c,2)])
        for owner in materials:f.call(0x423a50,this=owner,args=(0,))
        check((p.uint(controller+8)&65535)==1,'declared external controller pin survives')
        for owner in materials:f.call(p.uint(p.uint(owner)),this=owner,args=(1,))
        f.call(0x4545d0,this=animations,args=(1,))
    else:
        p.put_uint(r+0xc18c,material);p.put_uint(r+0xc1c4,mode=='batch-override')
        alternate=p.allocate(44)
        alternate_words=[0,1,0,0,0,0,7,6,5,17,1]
        for i,v in enumerate(alternate_words):p.put_uint(alternate+4*i,v)
        p.put_uint(r+0xc1cc,alternate)
        for i in range(1,11):p.put_uint(r+0xc71c+4*i,i%2)
        for i in range(256):p.put_uint(r+0xe4f4+4*i,0xa5a5a5a5)
        for i in range(11):p.put_uint(r+0xc868+4*i,0xcccccccc)
        p.put_floats(r+0xe4a4,tuple((i+1)/16 for i in range(16))+(8.,));p.put_uint(r+0xe4e8,77);p.put_uint(r+0xe4ec,88)
        for iteration in range(4):
            if iteration==2:p.put_uint(material+0x18+8*4,6)
            if iteration==3:p.put_uint(material+0x18+4,1);p.put_uint(alternate+8*4,2)
            f.events.clear();result=f.call(0x4bb890,this=r)&255;maximum=max(maximum,sum(p.visits.values()))
            check(result==1 and p.uint(r+0xc1c8)==material+0x18,'actual selected material source refreshed')
            desired=[p.uint((alternate if mode=='batch-override' and i%2 else material+0x18)+4*i) for i in range(1,11)]
            check(words(r+0xc86c,10)==desired,'actual batch dispatch including forced lighting refresh')
            captures.append([iteration,result,words(r+0xc868,11),words(r+0xe4a4,17),p.uint(r+0xe4e8),p.uint(r+0xe4ec),list(f.events)])
        cleanup(f,materials)
    cleanup(f,());check(set(f.allocations)==set(f.freed),'all actual native allocations freed')
    capture=[mode,captures]
    if not return_capture:print('MATERIAL_INSTALL_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original material installation {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
