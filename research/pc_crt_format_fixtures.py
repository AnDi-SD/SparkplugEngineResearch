"""Explicit bounded MSVCR71 sprintf %d/%i/%s/%x contracts, not host output."""
import re
from pc_stl_fixtures import read_cstring

def install_sprintf(p):
    base=0x340f0000;p.mu.mem_map(base,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def sprintf(machine):
        stack=machine.reg('ESP');destination=machine.uint(stack+4);fmt=read_cstring(machine,machine.uint(stack+8)).decode('latin1');index=0
        def substitute(match):
            nonlocal index
            if match.group()=='%%':return '%'
            value=machine.uint(stack+12+4*index);index+=1
            if match.group() in ('%d','%i'):return str(value if value<0x80000000 else value-0x100000000)
            if match.group()=='%s':return read_cstring(machine,value).decode('latin1')
            if match.group()=='%x':return format(value,'x')
            raise AssertionError('unsupported bounded printf format')
        result=re.sub(r'%%|%[disx]',substitute,fmt)
        if '%' in re.sub(r'%%|%[disx]','',fmt) or len(result)>255:raise AssertionError('bounded exact sprintf formats only')
        raw=result.encode('latin1');machine.mu.mem_write(destination,raw+b'\0');machine.fixture_return(eax=len(raw))
    p.put_uint(0x6d929c,base+16);p.seams[base+16]=sprintf
