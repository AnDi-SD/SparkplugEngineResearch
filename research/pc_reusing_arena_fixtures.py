"""Explicit reusable allocator within the fixture's declared fixed guest arena.

Not original allocator placement/reuse evidence. Large engine blocks use the
high end; caller-prepared storage and small blocks use the low end. Every
engine allocation has a generation and must be freed exactly once. No live
block moves, overlapping live ranges or map-on-fault. Default64KiB; an
explicit integration profile admits128KiB without changing execution limits.
"""
from pc_instruction_emulator import HEAP,ARENA_SIZE

class ReusingArena:
    def __init__(self,fixture,alignment=16):
        if alignment not in (8,16):raise ValueError('explicit 8/16-byte fixture placement only')
        self.alignment=alignment
        self.f=fixture;self.p=fixture.p;self.prefix=self.p.allocated
        self.capacity=getattr(self.p,'arena_size',ARENA_SIZE)
        self.free_ranges=[(HEAP+self.prefix,HEAP+self.capacity)]
        self.live={};self.history=[];self.released=set();self.next_generation=1
        self.reserved=self.prefix;self.peak_reserved=self.prefix;self.reuse_count=0;self.ever=[]
        self.p.allocate=self.allocate_raw
        # Later manager helpers install fixture.allocate at the alternate
        # CRT wrapper; keep those bindings on the same audited allocator.
        fixture.allocate=self.allocate_engine;fixture.free=self.free_engine
        for entry in (0x4123d0,0x4123f0,0x412400,0x417190):self.p.seams[entry]=self.allocate_engine
        self.p.seams[0x412420]=self.free_engine

    def reserve(self,size,engine,high=None):
        if size<=0:raise AssertionError('positive arena reservation')
        aligned=(size+self.alignment-1)&~(self.alignment-1)
        candidates=[i for i,(a,b) in enumerate(self.free_ranges) if b-a>=aligned]
        if not candidates:raise AssertionError(('fixed reusable arena exhausted',self.capacity,size,self.reserved,[b-a for a,b in self.free_ranges],
            'allocatorCaller',hex(self.p.uint(self.p.reg('ESP'))),'tail',[hex(a) for a in self.p.tail][-12:]))
        if high is None:high=engine and size>=4096
        index=candidates[-1] if high else min(candidates,key=lambda i:self.free_ranges[i][1]-self.free_ranges[i][0]) if getattr(self,'best_fit',False) else candidates[0]
        start,end=self.free_ranges[index];address=end-aligned if high else start
        pieces=[]
        if start<address:pieces.append((start,address))
        if address+aligned<end:pieces.append((address+aligned,end))
        self.free_ranges[index:index+1]=pieces
        if any(address<b and a<address+aligned for a,b in self.ever):self.reuse_count+=1
        self.ever.append((address,address+aligned))
        generation=self.next_generation;self.next_generation+=1
        self.live[address]=(aligned,engine,generation)
        self.reserved+=aligned;self.peak_reserved=max(self.peak_reserved,self.reserved)
        self.p.allocated=self.reserved
        if self.reserved>self.capacity or not HEAP+self.prefix<=address<address+aligned<=HEAP+self.capacity:
            raise AssertionError('arena absolute bound invariant')
        if address in self.f.freed:self.f.freed.remove(address)
        if not engine:self.f.allocations.pop(address,None)
        return address,generation

    def allocate_raw(self,size):
        address,_=self.reserve(size,False)
        # Caller-prepared inputs had zeroed fresh storage in the ordinary
        # fixture. Preserve that explicit input, even when reusing a range.
        self.p.mu.mem_write(address,bytes(size));return address

    def allocate_raw_high(self,size):
        """Declared caller backing placement; preserves contiguous scratch space."""
        address,_=self.reserve(size,False,True)
        self.p.mu.mem_write(address,bytes(size));return address

    def allocate_engine(self,p):
        size=p.uint(p.reg('ESP')+4)
        p.fixture_return(eax=self.allocate_owned(size))

    def allocate_owned(self,size):
        """Same contract for an explicitly declared external name owner."""
        if size>0x8000:raise AssertionError('unchanged32KiB engine allocation request cap')
        address,generation=self.reserve(max(1,size),True)
        self.p.mu.mem_write(address,b'\xcc'*max(1,size));self.f.allocations[address]=size
        self.f.requests.append((address,size));self.history.append((generation,address,size))
        return address

    def free_engine(self,p):
        address=p.uint(p.reg('ESP')+4)
        if address:self.release_owned(address)
        p.fixture_return()

    def release_owned(self,address):
        self.release(address,True)

    def release_raw(self,address):
        """Explicit caller input lifetime, never an engine free substitute."""
        self.release(address,False)

    def retire_initial_stream_inputs(self):
        """End the exact ReaderFixture raw32-byte stream +64-byte vtable input.

        Only legal after the final completed read. These96 bytes predate this
        allocator; verify their exact original layout before admitting reuse.
        No engine allocation or still-needed stream may be retired this way.
        """
        if (self.prefix!=96 or self.f.stream!=HEAP or self.p.uint(HEAP)!=HEAP+32
                or any(a<HEAP+96 for a in self.live)):
            raise AssertionError('exact initial external stream/vtable prefix required')
        generation=self.next_generation;self.next_generation+=1
        self.live[HEAP]=(96,False,generation);self.ever.append((HEAP,HEAP+96))
        self.prefix=0;self.f.stream=0;self.release_raw(HEAP)

    def release(self,address,expected_engine):
        if address:
            if address not in self.live:raise AssertionError('free requires a currently live allocation')
            aligned,engine,generation=self.live.pop(address)
            if engine!=expected_engine or generation in self.released:raise AssertionError('matching allocation owner and exactly-once free')
            if engine:self.released.add(generation);self.f.freed.append(address)
            self.reserved-=aligned;self.p.allocated=self.reserved
            ranges=sorted([*self.free_ranges,(address,address+aligned)]);merged=[]
            for start,end in ranges:
                if merged and start<merged[-1][1]:raise AssertionError('overlapping free ranges')
                if merged and start==merged[-1][1]:merged[-1]=(merged[-1][0],end)
                else:merged.append((start,end))
            self.free_ranges=merged

    def assert_engine_released(self):
        if any(engine for _,engine,_ in self.live.values()):raise AssertionError('live engine allocations remain')
        if self.released!={g for g,_,_ in self.history}:raise AssertionError('all engine generations must be accounted for')
