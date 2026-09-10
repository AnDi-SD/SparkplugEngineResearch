"""Bounded MSVCR71 strncpy/strncat contracts used by original diagnostics."""
from pc_stl_fixtures import read_cstring

def install_crt_string(p):
    base=0x34130000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def arguments(m):return [m.uint(m.reg('ESP')+4+4*i) for i in range(3)]
    def prefix(m,source,n):
        # Neither strncpy nor strncat requires a terminator within the first n
        # source bytes. Do not inspect byte n or read source when n is zero.
        result=bytearray()
        for i in range(n):
            value=bytes(m.mu.mem_read(source+i,1))
            if value==b'\0':break
            result.extend(value)
        return bytes(result)
    def strncpy(m):
        destination,source,n=arguments(m)
        if n>4096:raise AssertionError('bounded strncpy')
        data=prefix(m,source,n)
        if n:m.mu.mem_write(destination,data+b'\0'*(n-len(data)))
        m.fixture_return(eax=destination)
    def strncat(m):
        destination,source,n=arguments(m)
        if n>4096:raise AssertionError('bounded strncat')
        before=read_cstring(m,destination);after=prefix(m,source,n)
        if len(before)+len(after)>4096:raise AssertionError('bounded strncat total')
        m.mu.mem_write(destination+len(before),after+b'\0');m.fixture_return(eax=destination)
    for i,(iat,fn) in enumerate(((0x6d9328,strncpy),(0x6d9320,strncat)),1):
        target=base+i*16;p.put_uint(iat,target);p.seams[target]=fn
