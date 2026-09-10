"""Explicit locale/OS responses for bounded ASCII regex specimens.

Only identified imports are supplied. ASCII classification/case conversion
follow the documented C1/LCMAP fields. High-byte classification is an opaque
all-zero test input, not a claim about any Windows codepage; specimens must
remain ASCII. OSVERSIONINFOA is a declared NT5.1 response, not host discovery.
"""
import struct
from pc_stl_fixtures import read_cstring

def install_locale_inputs(p,*,critical_only=False):
    base=0x34150000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    locks={};calls=[]
    def args(m,n):return [m.uint(m.reg('ESP')+4+4*i) for i in range(n)]
    def critical(m,op):
        ptr,=args(m,1);calls.append(op)
        if op=='init':
            if ptr in locks:raise AssertionError('duplicate initialized critical section')
            locks[ptr]=0
        elif op=='enter':locks[ptr]+=1
        elif op=='leave':
            if locks[ptr]<=0:raise AssertionError('unbalanced critical section')
            locks[ptr]-=1
        else:
            if locks[ptr]:raise AssertionError('destroy entered critical section')
            del locks[ptr]
        m.fixture_return(4)
    def version(m):
        out,=args(m,1)
        if m.uint(out)!=148:raise AssertionError('exact OSVERSIONINFOA')
        m.mu.mem_write(out,struct.pack('<5I',148,5,1,2600,2)+bytes(128));m.fixture_return(4,eax=1)
    def locale(m):m.fixture_return(eax=0x409)
    def ctype(m):
        locale,kind,src,n,out=args(m,5)
        if locale!=0x409 or kind!=1 or n!=256 or bytes(m.mu.mem_read(src,n))!=bytes(range(256)):
            raise AssertionError('declared complete ASCII classifier input')
        values=[]
        for c in range(256):
            v=0
            if 65<=c<=90:v|=0x101
            if 97<=c<=122:v|=0x102
            if 48<=c<=57:v|=4
            if c in (9,10,11,12,13,32):v|=8
            if 33<=c<=126 and not chr(c).isalnum():v|=16
            if c<32 or c==127:v|=32
            if c in (9,32):v|=64
            if c in b'0123456789ABCDEFabcdef':v|=128
            values.append(v)
        m.mu.mem_write(out,struct.pack('<256H',*values));calls.append('ctype');m.fixture_return(20,eax=1)
    def mapstring(m):
        locale,flags,src,n,out,capacity=args(m,6)
        if locale!=0x409 or flags!=0x100 or n!=256 or capacity!=256:
            raise AssertionError('declared ASCII lowercase mapping')
        data=bytes(m.mu.mem_read(src,n));m.mu.mem_write(out,data.lower());calls.append('lower');m.fixture_return(24,eax=n)
    def multibyte(m):
        cp,flags,src,n,out,capacity=args(m,6)
        if cp!=0 or flags not in (0,1) or n!=0xffffffff:raise AssertionError('bounded ASCII MultiByteToWideChar')
        data=read_cstring(m,src)
        if any(c>=128 for c in data):raise AssertionError('non-ASCII conversion outside input contract')
        wide=b''.join(bytes((c,0)) for c in data)+b'\0\0';count=len(wide)//2
        if capacity:
            if capacity<count:raise AssertionError('insufficient supplied wide capacity')
            m.mu.mem_write(out,wide)
        m.fixture_return(24,eax=count)
    def wide(m):
        cp,flags,src,n,out,capacity,default,used=args(m,8)
        if cp or flags or n!=0xffffffff or default or used:raise AssertionError('bounded ASCII WideCharToMultiByte')
        data=bytearray()
        for i in range(2048):
            c=int.from_bytes(m.mu.mem_read(src+2*i,2),'little')
            if c>=128:raise AssertionError('non-ASCII wide conversion outside input contract')
            data.append(c)
            if not c:break
        else:raise AssertionError('bounded wide terminator')
        if capacity:
            if capacity<len(data):raise AssertionError('insufficient byte capacity')
            m.mu.mem_write(out,bytes(data))
        m.fixture_return(32,eax=len(data))
    def space(m):
        c,=args(m,1);m.fixture_return(eax=int(c in (9,10,11,12,13,32)))
    entries=[(0x6d9140,version),(0x6d9100,locale),(0x6d90fc,ctype),(0x6d9104,mapstring),
             (0x6d9120,multibyte),(0x6d9108,wide),(0x6d92f8,space)]
    # Reuse the same reviewed single-thread lock bookkeeping independently;
    # do not install locale/version/string responses for unrelated probes.
    if critical_only:entries=[]
    entries += [(iat,lambda m,o=op:critical(m,o)) for iat,op in ((0x6d911c,'init'),(0x6d910c,'enter'),(0x6d9110,'leave'),(0x6d9118,'delete'))]
    for i,(iat,fn) in enumerate(entries,1):
        target=base+16*i;p.put_uint(iat,target);p.seams[target]=fn
    return locks,calls
