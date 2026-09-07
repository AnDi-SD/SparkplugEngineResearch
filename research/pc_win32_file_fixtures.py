"""Explicit read-only Win32 file contract in guest memory, no host API calls.

Only the pristine PE's five identified KERNEL32 imports are provided. The
original PC stream and package-manager bodies continue to execute. Handles
refer exclusively to immutable supplied bytes, never to OS handles.
"""
from pc_stl_fixtures import read_cstring


class Win32FileInput:
    def __init__(self,p,data,name=b'fixture.rfx'):
        if len(data)>0x8000:raise ValueError('unchanged bounded file input')
        self.p=p;self.data=bytes(data);self.name=bytes(name);self.events=[]
        self.position=0;self.opened=False;self.handle=0x10203040
        base=0x34120000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
        for i,(iat,fn) in enumerate(((0x6d90c8,self.create),(0x6d90c0,self.read),
                (0x6d90d0,self.size),(0x6d916c,self.seek),(0x6d9168,self.close)),1):
            target=base+16*i;p.put_uint(iat,target);p.seams[target]=fn

    def args(self,p,n):return [p.uint(p.reg('ESP')+4+4*i) for i in range(n)]
    def require(self,handle):
        if handle!=self.handle or not self.opened:raise AssertionError('live declared file handle required')
    def create(self,p):
        name,access,share,security,creation,flags,template=self.args(p,7)
        if read_cstring(p,name)!=self.name or self.opened or (access,share,security,creation,flags,template)!=(0x80000000,3,0,3,1,0):
            raise AssertionError('exact read-only file open contract')
        self.opened=True;self.position=0;self.events.append(['open',self.name.decode('ascii'),access,share,creation,flags])
        p.fixture_return(28,eax=self.handle)
    def read(self,p):
        handle,out,count,actual,overlapped=self.args(p,5);self.require(handle)
        if overlapped or count>0x8000:raise AssertionError('bounded synchronous file read')
        n=min(count,len(self.data)-self.position)
        if n:p.mu.mem_write(out,self.data[self.position:self.position+n])
        p.put_uint(actual,n);self.events.append(['read',self.position,count,n]);self.position+=n;p.fixture_return(20,eax=1)
    def size(self,p):
        handle,high=self.args(p,2);self.require(handle)
        if high:p.put_uint(high,0)
        self.events.append(['size',len(self.data)]);p.fixture_return(8,eax=len(self.data))
    def seek(self,p):
        handle,offset,high,method=self.args(p,4);self.require(handle)
        if high or method>2:raise AssertionError('bounded 32-bit seek')
        offset=offset if offset<0x80000000 else offset-0x100000000
        pos=(0,self.position,len(self.data))[method]+offset
        if not 0<=pos<=len(self.data):raise AssertionError('seek outside declared file')
        self.events.append(['seek',offset,method,pos]);self.position=pos;p.fixture_return(16,eax=pos)
    def close(self,p):
        handle,=self.args(p,1);self.require(handle);self.opened=False
        self.events.append(['close']);p.fixture_return(4,eax=1)
