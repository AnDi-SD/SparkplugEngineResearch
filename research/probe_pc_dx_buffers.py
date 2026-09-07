#!/usr/bin/env python3
"""Actual DX wrapper/combiner code against explicit bounded COM inputs, no GPU."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

class BufferFixture(LifetimeFixture):
    def __init__(self,mode,renderer_extent=0xc9ec,device_vtable_size=0x70):
        super().__init__();self.mode=mode;self.events=[];self.buffers={}
        p=self.p
        self.renderer=p.allocate(renderer_extent);self.device=p.allocate(4);vt=p.allocate(device_vtable_size)
        p.put_uint(self.device,vt);p.put_uint(self.renderer+0xc9e8,self.device);p.put_uint(0x75db68,self.renderer)
        p.mu.mem_map(0x34060000,0x1000,p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        for offset,address,kind in ((0x68,0x34060010,'vertex'),(0x6c,0x34060020,'index')):
            p.put_uint(vt+offset,address)
            p.seams[address]=lambda p,k=kind:self.create(p,k)
        self.buffer_table=p.allocate(0x34)
        for offset,address,callback in ((8,0x34060030,self.release),(0x2c,0x34060040,self.lock),(0x30,0x34060050,self.unlock)):
            p.put_uint(self.buffer_table+offset,address);p.seams[address]=callback

    def create(self,p,kind):
        sp=p.reg('ESP');args=tuple(p.uint(sp+4+i*4) for i in range(7))
        device,size,usage,format_,pool,out,shared=args
        check(device==self.device and shared==0,'COM Create uses device this and null shared handle')
        check(0<size<=1024,'explicit bounded buffer size')
        self.events.append(('create',kind,size,usage,format_,pool))
        if self.mode=='create-failure':p.put_uint(out,0);p.fixture_return(28,eax=0x80004005);return
        obj=p.allocate(4);data=p.allocate(size);p.put_uint(obj,self.buffer_table)
        self.buffers[obj]={'kind':kind,'data':data,'size':size,'refs':1,'locks':0}
        p.put_uint(out,obj);p.fixture_return(28,eax=0)

    def release(self,p):
        obj=p.uint(p.reg('ESP')+4);b=self.buffers[obj]
        check(b['refs']>0,'COM object not already released')
        b['refs']-=1;self.events.append(('release',b['kind']))
        p.fixture_return(4,eax=b['refs'])

    def lock(self,p):
        sp=p.reg('ESP');obj,offset,size,out,flags=(p.uint(sp+4+i*4) for i in range(5))
        b=self.buffers[obj];check((offset,size,flags)==(0,0,0),'whole-buffer COM Lock flags and offsets')
        self.events.append(('lock',b['kind']))
        if self.mode=='lock-failure':p.fixture_return(20,eax=0x80004005);return
        b['locks']+=1;p.put_uint(out,b['data']);p.fixture_return(20,eax=0)

    def unlock(self,p):
        obj=p.uint(p.reg('ESP')+4);b=self.buffers[obj]
        check(b['locks']>0,'COM unlock follows explicit successful lock')
        b['locks']-=1;self.events.append(('unlock',b['kind']))
        p.fixture_return(4,eax=0x80004005 if self.mode=='unlock-failure' else 0)

def main(mode):
    if mode not in {'constructors','wrappers','create-failure','combiner','lock-failure','unlock-failure'}:
        raise ValueError('explicit bounded buffer mode required')
    f=BufferFixture(mode);p=f.p
    if mode in {'constructors','wrappers','create-failure'}:
        vertex=f.call(0x4b1e30);index=f.call(0x4b2010)
        print('WRAPPER CTORS',hex(vertex),hex(index),f.words(vertex,0x10,0x20),f.words(index,0x10,0x1c),flush=True)
        check(f.allocations[vertex]==0x20 and f.allocations[index]==0x1c,'actual factory sizes')
        check(p.uint(vertex)==0x6f05c4 and p.uint(index)==0x6f05f8,'actual wrapper vtables')
        check(p.uint(vertex+0x10)==0 and p.uint(index+0x10)==0,'constructors initialize COM pointers')
        if mode!='constructors':
            a=f.call(0x4b1f60,this=vertex,args=(96,8,0x112,1))&255
            b=f.call(0x4b2140,this=index,args=(12,8,0x65,1))&255
            check(a==b==1,'wrappers return true even when COM Create reports failure')
            check((p.uint(vertex+0x14),p.uint(vertex+0x1c),p.uint(index+0x18))==(0x112,96,12),'metadata committed regardless of HRESULT')
            check(f.events==[('create','vertex',96,8,0x112,1),('create','index',12,8,0x65,1)],'exact Create argument order')
            check(bool(p.uint(vertex+0x10))==(mode=='wrappers') and bool(p.uint(index+0x10))==(mode=='wrappers'),'COM result pointers retained without invented resource')
        f.call(0x4b1fa0,this=vertex,args=(1,));f.call(0x4b2180,this=index,args=(1,))
    else:
        combiner=p.allocate(0x2c);p.mu.mem_write(combiner,b'\xcc'*0x2c)
        check(f.call(0x4a9610,this=combiner)==combiner,'actual combiner constructor returns this')
        check(p.uint(combiner+4)==0xcccccccc and f.words(combiner,8,0x2c)==[0]*9,'constructor preserves target count but clears rest')
        result=f.call(0x4a96c0,this=combiner,args=(0x112,6,192,12,0xdeadbeef))&255
        print('COMBINER INIT',mode,result,f.words(combiner,4,0x2c),'events',f.events,'instructions',sum(p.visits.values()),flush=True)
        check(result==1,'combiner returns true including unchecked failed Lock')
        check(f.events==[('create','index',12,8,0x65,1),('create','vertex',192,8,0x112,1),('lock','vertex'),('lock','index')],'actual Create/Lock sequence')
        check(f.words(combiner,4,0x18)==[6,0,0x112,192,12],'combiner initialization metadata')
        check(p.uint(p.uint(combiner+0x18)+8)&0xffff==1 and p.uint(p.uint(combiner+0x1c)+8)&0xffff==1,'combiner owns one intrusive reference per wrapper')
        if mode=='lock-failure':
            check(p.uint(combiner+0x20)==p.uint(combiner+0x24)==0,'failed locks leave null cursors; no copy attempted')
        else:
            vb=p.uint(combiner+0x20);ib=p.uint(combiner+0x24)
            f.call(0x4a98e0,this=combiner,args=(3,96,3,6))
            check(f.words(combiner,0x20,0x2c)==[vb+96,ib+6,3],'partial commit cursor/index accounting')
            check(f.call(0x4a95e0,this=combiner)&255==0 and len(f.events)==4,'partial batch stays locked')
            f.call(0x4a98e0,this=combiner,args=(3,96,3,6))
            check(f.call(0x4a95e0,this=combiner)&255==1,'exact count is full')
            check(f.events[-2:]==[('unlock','vertex'),('unlock','index')],'exact-full commit unlocks vertex then index; HRESULT ignored')
        f.call(0x4a9640,this=combiner)
        check(f.events[-2:]==[('release','index'),('release','vertex')],'combiner destruction releases index then vertex')
    check(all(b['refs']==0 for b in f.buffers.values()),'all explicit COM resources released')
    check(set(f.allocations)==set(f.freed),'all actual wrapper allocations freed')
    print(f'PASS {checks}/{checks}: original DX buffers/combiner {mode}; synthetic COM boundary, not GPU evidence')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
