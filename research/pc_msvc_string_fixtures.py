"""Identified imported MSVCP71 string operations, explicit28-byte DLL ABI.

These are external-library contracts. In-image engine string algorithms are
not intercepted. Bounded non-overflowing char/wchar string values only; native
DLL allocation strategy is not claimed. All fixture allocations are tracked.
"""
from pc_stl_fixtures import read_cstring

def install_msvc_strings(f):
    p=f.p;base=0x34140000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def readz(address,width):
        if width==1:return read_cstring(p,address)
        result=bytearray()
        for i in range(2048):
            word=bytes(p.mu.mem_read(address+2*i,2))
            if word==b'\0\0':return bytes(result)
            result.extend(word)
        raise AssertionError('bounded wide string')
    def value(obj,width):
        n=p.uint(obj+20);cap=p.uint(obj+24)
        if n>cap or n*width>4096:raise AssertionError('valid bounded DLL string')
        ptr=p.uint(obj+4) if cap>=16//width else obj+4
        return bytes(p.mu.mem_read(ptr,n*width)) if n else b''
    def free(obj,width):
        if p.uint(obj+24)>=16//width:
            address=p.uint(obj+4)
            if address not in f.allocations or address in f.freed:raise AssertionError('DLL string owns live tracked buffer')
            f.freed.append(address)
    def setvalue(obj,data,width,construct):
        if len(data)%width or len(data)>4096:raise AssertionError('bounded DLL string value')
        if not construct:free(obj,width)
        n=len(data)//width;cap=16//width-1
        p.mu.mem_write(obj,bytes(28))
        if n>cap:
            cap=n;ptr=p.allocate(len(data)+width);f.allocations[ptr]=len(data)+width
            f.requests.append((ptr,len(data)+width));p.put_uint(obj+4,ptr)
        else:ptr=obj+4
        p.mu.mem_write(ptr,data+bytes(width));p.put_uint(obj+20,n);p.put_uint(obj+24,cap)
    def execute(m,width,operation):
        obj=m.reg('ECX');sp=m.reg('ESP');arg=lambda i:m.uint(sp+4+4*i)
        n=0
        if operation=='dtor':free(obj,width);setvalue(obj,b'',width,True)
        elif operation=='empty':setvalue(obj,b'',width,True)
        elif operation in ('range','cstring','copy','assignz','assign','char'):
            n=2 if operation=='range' else 1
            if operation=='range':
                start,end=arg(0),arg(1)
                if end<start or end-start>4096:raise AssertionError('bounded DLL string range')
                data=bytes(m.mu.mem_read(start,end-start)) if end>start else b''
            elif operation in ('copy','assign'):data=value(arg(0),width)
            elif operation=='char':data=(arg(0)&((1<<(width*8))-1)).to_bytes(width,'little')
            else:data=readz(arg(0),width)
            setvalue(obj,data,width,operation in ('range','cstring','copy'))
        elif operation=='equals':
            m.fixture_return(eax=int(value(arg(0),width)==readz(arg(1),width)));return
        else:raise AssertionError(operation)
        m.fixture_return(n*4,eax=obj)
    entries=((0x6d9184,1,'empty'),(0x6d91a4,2,'empty'),(0x6d91dc,1,'dtor'),(0x6d91d4,2,'dtor'),
             (0x6d91b0,1,'assignz'),(0x6d91ac,2,'assignz'),(0x6d91b8,1,'char'),(0x6d9180,1,'assign'),
             (0x6d91d8,1,'range'),(0x6d91a8,2,'range'),(0x6d91e4,1,'cstring'),
             (0x6d91e0,1,'copy'),(0x6d91b4,1,'equals'))
    for i,(iat,width,op) in enumerate(entries,1):
        target=base+i*16;p.put_uint(iat,target);p.seams[target]=lambda m,w=width,o=op:execute(m,w,o)
