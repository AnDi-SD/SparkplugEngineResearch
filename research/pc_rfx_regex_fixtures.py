"""Original RFX regexes after a completed independent shared-library warmup.

Cold ID-regex construction hit the instruction/time cap and remains unclosed.
No capped state is resumed. Each fresh fixture first completes A(.*?)B, which
initializes shared library tables, then constructs both exact shipped regexes
in ordinary bounded calls. All regex/search/library bodies remain original.
"""
import re
from pc_stl_fixtures import install_char_traits,read_cstring
from pc_crt_memory_fixtures import install_crt_memory
from pc_crt_string_fixtures import install_crt_string
from pc_crt_format_fixtures import install_sprintf
from pc_msvc_string_fixtures import install_msvc_strings
from pc_locale_input_fixtures import install_locale_inputs

class RFXRegexInputs:
    def __init__(self,f):
        self.f=f;p=f.p;install_char_traits(p);install_crt_memory(f)
        install_crt_string(p);install_sprintf(p);install_msvc_strings(f)
        self.locks,self.localeCalls=install_locale_inputs(p);self.scanCalls=[]
        p.mu.mem_write(0x73ff60,b'\0')
        base=0x34160000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
        def sscanf(m):
            textptr,fmtptr,out=[m.uint(m.reg('ESP')+4+4*i) for i in range(3)]
            text=read_cstring(m,textptr)
            if read_cstring(m,fmtptr)!=b'%x':raise AssertionError('identified hexadecimal scanf only')
            if not text.strip(b' \t\r\n\v\f'):
                self.scanCalls.append([text.decode('ascii'),-1,None]);m.fixture_return(eax=0xffffffff);return
            match=re.match(rb'[ \t\r\n\v\f]*([+-]?(?:0[xX])?[0-9a-fA-F]+)',text)
            value=None
            if match:
                parsed=int(match.group(1),16)
                if abs(parsed)>0xffffffff:raise AssertionError('scanf integer overflow outside declared contract')
                value=parsed&0xffffffff;m.put_uint(out,value)
            self.scanCalls.append([text.decode('ascii'),int(bool(match)),value]);m.fixture_return(eax=int(bool(match)))
        p.put_uint(0x6d93ac,base+16);p.seams[base+16]=sscanf
        # Exact MSVCP71 numeric_limits<int>::max() import.
        p.put_uint(0x6d9188,base+32);p.seams[base+32]=lambda m:m.fixture_return(eax=0x7fffffff)
        alloc=p.allocate(4);self.warm=p.allocate(64);small=p.allocate(8);p.mu.mem_write(small,b'A(.*?)B\0')
        self.initializationInstructions=[]
        for obj,pattern in ((self.warm,small),(0x765678,0x6f3e64),(0x7656b8,0x6f3e98)):
            f.call(0x4d0f00,this=obj,args=(pattern,0x8107,alloc))
            self.initializationInstructions.append(sum(p.visits.values()))
    def close(self):
        for obj in (0x7656b8,0x765678,self.warm):self.f.call(0x4d0f20,this=obj)
        if self.locks:raise AssertionError('all original regex critical sections destroyed')
