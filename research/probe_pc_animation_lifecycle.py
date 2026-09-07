#!/usr/bin/env python3
"""Original PC animation/track lifetime in bounded guest memory.

Only allocation/free and non-throwing FS:0 storage are synthetic. The Windows
loader, game loop, exceptions and host APIs are not executed. Allocator fixtures
record requests; they do not prove native allocation failure/rollback behavior.
"""
from pathlib import Path
import math
import struct
import sys
from pc_instruction_emulator import PcInstructions, run_bounded, ARENA_SIZE

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

class LifetimeFixture:
    def __init__(self):
        self.p=PcInstructions(arena_size=getattr(self,'guest_arena_size',ARENA_SIZE))
        self.teb=self.p.fixture_seh_chain()
        self.allocations={}
        self.freed=[]
        self.requests=[]
        self.p.seams[0x4123d0]=self.allocate
        self.p.seams[0x412400]=self.allocate
        self.p.seams[0x412420]=self.free
        # std::vector's allocator can reach the CRT malloc wrapper directly,
        # bypassing 0x412400. Keep it synthetic too; never forward the IAT call.
        self.p.seams[0x417190]=self.allocate

    def allocate(self,p):
        size=p.uint(p.reg('ESP')+4)
        if size>0x8000:raise AssertionError('allocation exceeds bounded fixture')
        address=p.allocate(max(1,size))
        p.mu.mem_write(address,b'\xcc'*max(1,size))
        self.allocations[address]=size
        self.requests.append((address,size))
        p.fixture_return(eax=address)

    def free(self,p):
        address=p.uint(p.reg('ESP')+4)
        if address:
            check(address in self.allocations,'free points to explicit allocation')
            check(address not in self.freed,'no duplicate release')
            self.freed.append(address)
        p.fixture_return()

    def call(self,entry,this=0,args=()):
        self.p.run(entry,this=this,args=args)
        check(self.p.uint(self.teb)==0xffffffff,'SEH chain restored')
        return self.p.reg('EAX')

    def words(self,obj,start,end):
        return [self.p.uint(obj+i) for i in range(start,end,4)]

    def buffer(self,values):
        p=self.p
        address=p.allocate(len(values)*4)
        self.allocations[address]=len(values)*4
        p.put_floats(address,values)
        return address


def x87_value(p):
    top=(p.reg('FPSW')>>11)&7
    mantissa,exponent=p.mu.reg_read(getattr(p.xr,'UC_X86_REG_FP'+str(top)))
    if not mantissa:return 0.
    return math.ldexp(mantissa/(1<<63),(exponent&0x7fff)-16383)*(-1 if exponent&0x8000 else 1)


def constructors():
    f=LifetimeFixture();p=f.p
    first=f.call(0x41a090)
    check(f.allocations[first]==0x84,'animation exact allocation')
    check(p.uint(first)==0x6de6cc,'animation vtable')
    check(f.words(first,0x10,0x28)==[0]*6,'name/time/field18/tracks/count/capacity defaults')
    check(p.uint(first+0x28)==0xcccccccc,'vector helper state28 remains unwritten')
    check(f.words(first,0x2c,0x54)==[0]*10,'tags and seven buffers empty')
    check(f.words(first,0x58,0x84)==[0x6db8b8,0,0x40,0,0,0xffffffff,0x10,0,0,0,0x15],
          'descriptor pool full prefix')
    check(p.uint(first+0x54)==0xffffffff,'first debug table value with pristine table')
    debug=p.uint(0x75526c)
    check(debug in f.allocations and f.allocations[debug]==0x38,'real lazy debug manager factory')
    second=f.call(0x41a090)
    check(p.uint(second+0x54)==0xffff0000,'subsequent debug table value is not a constant default')
    check(p.uint(0x75526c)==debug,'debug instance reused')
    check(p.uint(first+8)==0xcccc0000,'base count zero but padding untouched')
    f.call(0x430130,this=second)
    f.call(0x430130,this=first)
    check(not f.freed,'empty animation dtor leaves externally owned debug singleton')


def track_storage():
    f=LifetimeFixture();p=f.p
    animation=f.call(0x41a090)
    first=f.call(0x42fed0,this=animation)
    check(p.uint(animation+0x20)==1 and p.uint(animation+0x24)==1,'first append count/capacity')
    check(p.uint(first)==0x6eaa24,'embedded track is spAnimTrack vtable')
    check(f.words(first,0x10,0x3c)==[0,0xffffffff]+[0]*9,'track name/binding/descriptors defaults')
    check(p.uint(first+0x3c)==0xcccccc00,'track ownership false; padding not reset')
    check(p.uint(first+0x40)==animation,'track owner animation')
    p.put_uint(first+0xc,0x12345678)
    second=f.call(0x42fed0,this=animation)
    moved=p.uint(animation+0x1c)
    check(second==moved+0x44 and p.uint(animation+0x24)==2,'append grows by exactly one')
    check(first in f.freed and p.uint(moved+0xc)==0x12345678,'old track bytes relocated without clone')
    check(p.uint(moved+0x40)==animation,'relocated owner unchanged')
    # Resize shrinking is tested only when every old capacity slot is constructed.
    # Native reserve-shrink iterates capacity, not count: a separate open hazard.
    f.call(0x430010,this=animation,args=(1,))
    check(p.uint(animation+0x20)==1 and p.uint(animation+0x24)==1,'shrink truncates active tail')
    check(p.uint(p.uint(animation+0x1c)+0xc)==0x12345678,'shrink preserves retained bytes')
    f.call(0x430130,this=animation)
    check(p.uint(animation+0x1c) in f.freed,'animation destructor frees track storage')


def named_copy():
    f=LifetimeFixture();p=f.p
    source=f.call(0x41a090);target=f.call(0x41a090)
    name=p.allocate(32)
    p.mu.mem_write(name+8,b'\x01animation-name\x00')
    p.put_uint(source+0x10,name)
    p.put_uint(source+0xc,0x12345678)
    p.put_uint(source+0x14,0x41400000)
    f.call(0x413120,this=source,args=(target,))
    check(p.reg('EAX')&0xff==1,'copy succeeds')
    check(p.uint(target+0x10)==name,'copy retains shared name, not blank/no-op')
    check(bytes(p.mu.mem_read(name+8,1))==b'\x02','shared name byte count increments')
    check(p.uint(target+0x14)==0,'animation duration not copied by inherited name slot')
    # Avoid attributing synthetic string release to the native string manager.
    p.put_uint(source+0x10,0);p.put_uint(target+0x10,0)
    f.call(0x430130,this=source);f.call(0x430130,this=target)


def standalone_tracks():
    f=LifetimeFixture();p=f.p
    base=f.call(0x493070)
    check(f.allocations[base]==0x14 and p.uint(base)==0x6ecba4,'spTrack exact size/vtable')
    check(p.uint(base+0x10)==0,'spTrack has only inherited name storage')
    f.call(0x493060,this=base)
    check(x87_value(p)==0,'spTrack duration virtual returns zero')
    f.call(0x48eaa0,this=base)
    f.call(0x493140,this=base,args=(1,))
    track=f.call(0x4791b0)
    check(f.allocations[track]==0x44 and p.uint(track)==0x6eaa24,'spAnimTrack standalone size/vtable')
    check(p.uint(track+0x40)==0xcccccccc,'standalone factory does not initialize owner')
    check(p.uint(track+0x14)==0xffffffff,'unbound sentinel belongs to derived spAnimTrack')
    f.call(0x478e30,this=track)
    check(x87_value(p)==0,'empty animation track duration')
    f.call(0x479a40,this=track,args=(1,))
    check(base in f.freed and track in f.freed,'standalone deleting destructors')


def descriptor_ownership():
    for old_ownership in (0,1):
        for new_ownership in (0,1):
            f=LifetimeFixture();p=f.p
            animation=f.call(0x41a090);track=f.call(0x42fed0,this=animation)
            times=f.buffer([0,3,8]);values=f.buffer([1,2,3,4,5,6,7,8,9])
            f.call(0x479830,this=track,args=(0,3,times,values,1,0,old_ownership))
            descriptor=p.uint(track+0x18)
            check(f.words(descriptor,0,16)==[3,1,times,values],'real pool descriptor populated')
            check(p.uint(animation+0x74)==1 and p.uint(animation+0x78)==63,'one pool block; 63 slots free')
            f.call(0x478e30,this=track)
            check(x87_value(p)==8,'duration from final time')
            second_times=f.buffer([0,2,5]);second_values=f.buffer([2]*9)
            f.call(0x479830,this=track,args=(0,3,second_times,second_values,1,0,new_ownership))
            check((times in f.freed)==bool(new_ownership),'replacement uses incoming ownership flag for old times')
            check((values in f.freed)==bool(new_ownership),'replacement uses incoming ownership flag for old values')
            check(p.uint(animation+0x78)==63,'replacement returns descriptor before reallocation')
            f.call(0x479760,this=track)
            check((second_times in f.freed)==bool(new_ownership),'release uses current track-wide flag')
            check((second_values in f.freed)==bool(new_ownership),'release frees values according to current flag')
            check(p.uint(track+0x18)!=0,'release helper leaves stale descriptor pointer')
            check(p.uint(animation+0x5c)==0 and p.uint(animation+0x74)==0
                  and p.uint(animation+0x78)==0 and p.uint(animation+0x7c)!=0,
                  'last descriptor release retires block into one spare cache')
            # Native helper is not an idempotent public Reset: clear the stale
            # pointer in fixture before running destructor a second time.
            p.put_uint(track+0x18,0)
            pool_blocks=[address for address,size in f.requests if size>0x100]
            f.call(0x430130,this=animation)
            check(pool_blocks and all(a in f.freed for a in pool_blocks),'animation owns/releases descriptor pool block')


def tag_order():
    f=LifetimeFixture();p=f.p
    animation=f.call(0x41a090)
    tags=[]
    for ordinal,time in enumerate((3.,1.,3.,0.,2.)):
        tag=p.allocate(0x1c);f.allocations[tag]=0x1c
        f.call(0x40e910,this=tag)
        p.put_uint(tag,0x6e0ae4);p.put_uint(tag+0x10,0)
        p.put_floats(tag+0x14,[time]);p.put_uint(tag+0x18,ordinal)
        f.call(0x4305f0,this=animation,args=(tag,));tags.append(tag)
    begin=p.uint(animation+0x2c);end=p.uint(animation+0x30)
    check(end-begin==20,'five owned tag pointers')
    ordered=[p.uint(begin+i*4) for i in range(5)]
    check(ordered==[tags[i] for i in (3,1,4,0,2)],'tag insertion stable ascending time')
    check([p.uint(t+0x18) for t in ordered]==[3,1,4,0,2],'wire ordinals retained after sorting')
    f.call(0x430130,this=animation)
    check(all(tag in f.freed for tag in tags),'animation deletes each owned tag')


def main():
    constructors();track_storage();named_copy();standalone_tracks();descriptor_ownership();tag_order()
    print(f'PASS {checks}/{checks}: PC animation lifecycle guest checks')
    return 0

if __name__=='__main__':
    if sys.argv[1:]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
