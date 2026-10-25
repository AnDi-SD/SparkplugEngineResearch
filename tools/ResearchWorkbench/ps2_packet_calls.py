"""Opt-in typed external primitives for original PS2 packet-body probes.

Only terminal heap callbacks,memset,memcpy and an ignored diagnostic call are
modeled. Packet producers,array-cookie helpers and list operations may not be
registered as successful callbacks. Original JAL sites,targets and RA are
verified; heap GP forwarding wrappers still execute original instructions.
The bump arena is explicit research environment,not a recovered game allocator.
"""

class PacketCallBoundaries:
    ALIGNED=0x20000100
    ALLOCATE=0x20000120
    FREE=0x20000140
    LIBC={'memcpy':0x4074f0,'memset':0x4076a8,'diagnostic':0x115b20}
    WRAPPERS={'aligned_allocate':(0x10d7d0,), 'allocate':(0x10d7f0,0x10d840,0x10d850), 'free':(0x10d7c0,0x10d810)}
    TAILS={0x10d7d0:0x10d7e0,0x10d7f0:0x10d800,0x10d840:0x10d844,
        0x10d850:0x10d860,0x10d7c0:0x10d7c4,0x10d810:0x10d814}

    def __init__(self,arena,windows,call_sites,*,maximum_calls=128,maximum_allocations=32):
        from ps2_scalar_prefix import pristine
        from capture_native_ranges import read_window
        if not isinstance(arena,(tuple,list)) or len(arena)!=2:raise ValueError('Explicit packet heap arena required')
        a,n=arena
        if type(a) is not int or type(n) is not int or a%16 or not 16<=n<=65536 or a<0 or a+n>0x100000000:
            raise ValueError('Aligned bounded packet heap arena required')
        if not isinstance(windows,(tuple,list)) or not 1<=len(windows)<=16:raise ValueError('Explicit primitive memory windows required')
        self.windows=[]
        for start,length,mode in windows:
            if (type(start) is not int or type(length) is not int or start<0 or not 0<length<=65536
                    or start+length>0x100000000 or mode not in ('r','rw')
                    or start<a+n and a<start+length
                    or any(start<b+m and b<start+length for b,m,_ in self.windows)):
                raise ValueError('Disjoint bounded primitive memory windows required')
            self.windows.append((start,length,mode))
        if (not isinstance(call_sites,dict) or not 1<=len(call_sites)<=64
                or type(maximum_calls) is not int or not 1<=maximum_calls<=1024
                or type(maximum_allocations) is not int or not 1<=maximum_allocations<=64):
            raise ValueError('Explicit bounded typed call sites/caps required')
        raw,sections=pristine();self.sites={}
        for site,kind in call_sites.items():
            if type(site) is not int or site%4 or kind not in self.LIBC and kind not in self.WRAPPERS:
                raise ValueError('Only declared external primitive kinds allowed')
            code,_=read_window('ps2',raw,site,8,sections);word=int.from_bytes(code[:4],'little')
            target=((site+4)&0xf0000000)|((word&0x03ffffff)<<2)
            allowed=(self.LIBC[kind],) if kind in self.LIBC else self.WRAPPERS[kind]
            if word>>26!=3 or target not in allowed:raise ValueError('Original typed JAL target does not match primitive')
            self.sites[site]=dict(kind=kind,target=target,originalWords=code.hex())
        self.arena=(a,n);self.cursor=a;self.allocations={};self.maximum_calls=maximum_calls
        self.maximum_allocations=maximum_allocations;self.allocation_count=0;self.events=[];self.attached=False

    def attach(self,p,gp):
        from ps2_scalar_prefix import pristine
        from capture_native_ranges import read_window
        if self.attached:raise ValueError('Discard each primitive environment after one attachment')
        if type(gp) is not int or gp%4 or not 0x8000<=gp<0x100000000:raise ValueError('Explicit positive32 aligned GP required')
        a,n=self.arena
        for start,length in [(a,n)]+[(b,m) for b,m,_ in self.windows]:
            if any(start<x+y and x<start+length for x,y in p.ranges+((p.RETURN,4096),)):
                raise ValueError('Primitive data must not overlap original code or RETURN')
        for offset,target in ((-0x7f38,self.ALIGNED),(-0x7f3c,self.ALLOCATE),(-0x7f34,self.FREE)):
            at=gp+offset
            if any(x<=at<x+y for x,y in p.ranges+((p.RETURN,4096),)) or a<=at<a+n:
                raise ValueError('Heap GP table overlaps code or heap')
            p.map(at,4);p.put_uint(at,target)
        p.reg('GP',gp);p.map(a,n);p.write(a,b'\xa5'*n)
        raw,sections=pristine()
        for address in self.LIBC.values():
            p.map(address,4);p.write(address,read_window('ps2',raw,address,4,sections)[0])
        self.attached=True

    def permits(self,address,size,write=False):
        if size==0:return True
        if not 0<size<=65536 or address<0 or address+size>0x100000000:return False
        if any(a<=address and address+size<=a+n for a,n in self.allocations.items()):return True
        return any(a<=address and address+size<=a+n and (not write or mode=='rw') for a,n,mode in self.windows)

    def is_entry(self,address):return address in (self.ALIGNED,self.ALLOCATE,self.FREE,*self.LIBC.values())

    def plan(self,p,address):
        if address not in (self.ALIGNED,self.ALLOCATE,self.FREE,*self.LIBC.values()):return None
        if not self.attached or len(self.events)>=self.maximum_calls:raise ValueError('Primitive environment/call cap rejected')
        site=p.reg('RA')-8;declared=self.sites.get(site)
        if declared is None:raise ValueError('External primitive caller/RA not declared')
        kind,target=declared['kind'],declared['target']
        if kind in self.LIBC:
            if address!=self.LIBC[kind] or p.trace[-2:]!=[site,site+4]:raise ValueError('Direct primitive call trace mismatch')
        else:
            wanted={'aligned_allocate':self.ALIGNED,'allocate':self.ALLOCATE,'free':self.FREE}[kind]
            tail=self.TAILS[target]
            expected=[site,site+4]+list(range(target,tail+8,4))
            if address!=wanted or p.trace[-len(expected):]!=expected or p.uint(tail)!=0x00400008 or p.uint(tail+4)!=0:
                raise ValueError('Terminal heap wrapper trace mismatch')
        if p.read(site,8).hex()!=declared['originalWords']:raise ValueError('Original call words changed')
        values=[p.reg('A'+str(i)) for i in range(4)]
        arity={'aligned_allocate':2,'allocate':1,'free':1,'memset':3,'memcpy':3,'diagnostic':2}[kind]
        if any(v<0 or v>0xffffffff for v in values[:arity]):raise ValueError('Primitive ABI outside positive32 domain')
        a0,a1,a2,a3=values
        row=dict(kind=kind,externalAddress=f'{address:08X}',originalCallSite=f'{site:08X}',
            originalTarget=f'{target:08X}',arguments=values,returnPc=site+8,originalInstructionsBefore=len(p.trace))
        if kind in ('aligned_allocate','allocate'):
            alignment=a1 if kind=='aligned_allocate' else 16
            if not a0 or a0>65536 or alignment!=16 or self.allocation_count>=self.maximum_allocations:
                raise ValueError('Packet heap allocation outside declared domain')
            at=(self.cursor+15)&~15
            if at+a0>sum(self.arena):raise ValueError('Packet heap arena exhausted')
            row.update(pointer=at,size=a0,alignment=alignment)
        elif kind=='free':
            if a0 and a0 not in self.allocations:raise ValueError('Free requires a live exact heap pointer')
            row.update(pointer=a0,size=self.allocations.get(a0,0))
        elif kind=='memset':
            if not self.permits(a0,a2,True):raise ValueError('memset outside writable primitive window/allocation')
            row.update(destination=a0,size=a2,data=(bytes([a1&255])*a2).hex())
        elif kind=='memcpy':
            if not self.permits(a0,a2,True) or not self.permits(a1,a2):raise ValueError('memcpy outside primitive windows/allocations')
            if a2 and a0<a1+a2 and a1<a0+a2:raise ValueError('Overlapping memcpy outside qualified domain')
            row.update(destination=a0,source=a1,size=a2,data=p.read(a1,a2).hex())
        else:
            if site!=0x15fc08 or a0!=0x445f30:raise ValueError('Only ignored packet-size diagnostic boundary qualified')
        return row

    def validate_transfer(self,plan):
        row=plan['slot'] if plan['operation']=='BEQ/SWC1' else plan
        if row['operation'] in ('LWC1','SWC1'):
            if not self.permits(int(row['effectiveAddress'],16),4,row['operation']=='SWC1'):
                raise ValueError('Raw FPR transfer outside live heap allocation/primitive windows')

    def commit(self,p,row):
        kind=row['kind']
        if kind in ('aligned_allocate','allocate'):
            self.allocations[row['pointer']]=row['size'];self.cursor=row['pointer']+row['size'];self.allocation_count+=1
            p.reg('V0',row['pointer'])
        elif kind=='free':
            if row['pointer']:del self.allocations[row['pointer']]
        elif kind in ('memset','memcpy'):
            p.write(row['destination'],bytes.fromhex(row['data']));p.reg('V0',row['destination'])
        # free/diagnostic return values are not fabricated; their callers do
        # not consume V0. Only the explicit callback return PC is installed.
        p.reg('PC',row['returnPc']);self.events.append(row)
