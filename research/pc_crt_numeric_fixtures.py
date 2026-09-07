"""Explicit finite ASCII MSVCR71 atoi/atof imports; no host API forwarding."""
import math,re
from pc_stl_fixtures import read_cstring

def atoi(p):
    data=read_cstring(p,p.uint(p.reg('ESP')+4))
    match=re.match(rb'[ \t\r\n\v\f]*([+-]?[0-9]+)',data)
    value=int(match.group(1)) if match else 0
    if not -0x80000000<=value<=0x7fffffff:raise ValueError('CRT integer overflow outside bounded fixture')
    p.fixture_return(eax=value&0xffffffff)

def atof(p):
    data=read_cstring(p,p.uint(p.reg('ESP')+4))
    match=re.match(rb'[ \t\r\n\v\f]*([+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)',data)
    value=float(match.group(1)) if match else 0.
    if not math.isfinite(value):raise ValueError('nonfinite CRT conversion outside bounded fixture')
    p.fixture_push_x87(value);p.fixture_return()

def install_crt_numeric(p):
    base=0x340d0000;p.mu.mem_map(base,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    for offset,iat,handler in ((16,0x6d930c,atoi),(32,0x6d9308,atof)):
        p.put_uint(iat,base+offset);p.seams[base+offset]=handler
