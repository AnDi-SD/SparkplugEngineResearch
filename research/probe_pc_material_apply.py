#!/usr/bin/env python3
"""Bounded PC material/layer render-state consumer, external COM only.

Declared renderer backing and caches, never a whole constructor/device. All
original layer/renderer dispatch executes; no internal fake-success leaves.
"""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup

class ApplyFixture(PCWriteBytesFixture):
    def __init__(self,fail=False,renderer_size=0xf174,device_table_size=0x1b0):
        super().__init__();self.prepare_device(fail,renderer_size,device_table_size)

    def prepare_device(self,fail=False,renderer_size=0xf174,device_table_size=0x1b0,compact_device=False,renderer_storage=None):
        """Declare device inputs on an existing, completed fixture state."""
        p=self.p;self.events=[];self.fail=fail;self.active_index=None
        if renderer_size not in (0xf174,0xf2f8,0xf358,0xf364) or device_table_size not in (0x1b0,0x1b8):raise ValueError('declared consumer storage only; no raised arena/allocation caps')
        self.renderer=renderer_storage if renderer_storage is not None else p.allocate(renderer_size)
        p.mu.mem_map(0x34090000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
        # Optional pure external COM data placement in its existing interface
        # page. Renderer and every engine allocation remain in the64KiB arena.
        self.device=0x34090400 if compact_device else p.allocate(4)
        table=0x34090500 if compact_device else p.allocate(device_table_size)
        p.put_uint(self.renderer,0x6f2918);p.put_uint(self.renderer+0x18,0x6f28a0)
        p.put_uint(self.renderer+0xc9e8,self.device);p.put_uint(self.device,table);p.put_uint(0x75db68,self.renderer)
        for i,(slot,name,argc) in enumerate(((0xe4,'render',3),(0x10c,'stage',4),(0x114,'sampler',4),(0x104,'texture',3),(0xb0,'transform',3),(0xc4,'material',2),(0x170,'vertex-shader',2),(0x1ac,'pixel-shader',2))):
            address=0x34090010+i*0x10;p.put_uint(table+slot,address)
            p.seams[address]=lambda machine,n=name,c=argc:self.observe(machine,n,c)
    def observe(self,p,name,argc):
        args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
        if args[0]!=self.device or len(self.events)>=128:raise AssertionError('bounded declared COM device observer')
        event=[name,*args[1:]]
        if name=='render' and self.active_index is not None:event.extend([p.uint(self.renderer+0xc868+4*self.active_index),p.uint(self.renderer+0xe4f4+4*args[1])])
        self.events.append(event);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)

def main(mode,return_capture=False):
    if mode not in ('layer-default','layer-values','state-map','state-fail','state-poison','state-repeat','lighting0'):raise ValueError('only declared finite material state contract')
    f=ApplyFixture(mode=='state-fail');p=f.p;layer=f.call(0x460e50);output=p.allocate(36);checks=0;maximum=0;captures=[]
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    if mode=='state-poison':p.mu.mem_write(f.renderer+0xe4f4,b'\xa5'*1024) # explicit 256-entry input subset, not native total extent/default
    try:
        if mode.startswith('layer-'):
            texture=p.uint(layer+0x10)
            if mode=='layer-values':
                for i in range(9):p.put_uint(texture+0x10+4*i,0x10000000+i)
            for stage in (0,1,2,0xffffffff):
                p.mu.mem_write(output,b'\xcc'*36);result=f.call(0x423590,this=layer,args=(stage,output))&255;maximum=max(maximum,sum(p.visits.values()))
                values=[p.uint(output+4*i) for i in range(9)]
                check(result==1 and values==[p.uint(texture+0x10+4*i) for i in range(9)] and not f.events,'stage unused, exact nine raw words, no device calls')
                captures.append([stage,result,values])
        else:
            inputs=[(8,0)] if mode=='lighting0' else [(i,v) for i,values in ((0,(17,)),(1,(0,1,2)),(2,(0,1)),(3,(0,1,2)),(4,(0,1,2)),(5,(0,1,2)),(6,range(8)),(7,range(8)),(9,(0,255,0x12345678)),(10,range(8)),(11,(19,))) for v in values]
            for index,value in inputs:
                f.active_index=index
                for repeat in range(2 if mode=='state-repeat' else 1):
                    f.events.clear();result=f.call(0x4b0ad0,this=f.renderer,args=(index,value))&255;maximum=max(maximum,sum(p.visits.values()))
                    check(result==(0 if index in (0,11) else 1) and p.uint(f.renderer+0xc868+index*4)==value,'native writes raw common cache even before rejected index')
                    if repeat:check(not f.events,'lower-level device cache suppresses identical repeat')
                    captures.append([index,value,result,p.uint(f.renderer+0xc868+index*4),list(f.events)])
    except Exception:
        print('MATERIAL_APPLY_STOP',json.dumps(f.events),'tail',[hex(a) for a in p.tail],flush=True)
        raise
    cleanup(f,(layer,));check(set(f.allocations)==set(f.freed),'all actual allocations freed')
    capture=[mode,captures]
    if not return_capture:print('MATERIAL_APPLY_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: bounded original material apply {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
