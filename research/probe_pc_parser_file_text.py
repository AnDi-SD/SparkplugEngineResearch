#!/usr/bin/env python3
"""Original4D0A30 file-to-owned-text helper through real PC stream bodies."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import install_char_traits
from pc_win32_file_fixtures import Win32FileInput
from pc_crt_string_fixtures import install_crt_string
from pc_crt_format_fixtures import install_sprintf
from probe_pc_function_eval import cleanup

CASES={'tiny':b'<Root>tiny\r\n</Root>','nul':b'a\0b\xffc',
       'growth-before':b'x'*4996,'growth-at':b'x'*5000,'growth-after':b'x'*5001,'empty':b''}

def main(mode,return_capture=False):
    data=CASES[mode];f=PCWriteBytesFixture();p=f.p;install_char_traits(p);install_crt_string(p);install_sprintf(p);api=Win32FileInput(p,data)
    # Explicit optional debug-output flag, actual47D8F0 disabled branch.
    # Error construction/formatting/dispatch still execute; no host output.
    p.mu.mem_write(0x73ff60,b'\0')
    name=p.allocate(len(api.name)+1);p.mu.mem_write(name,api.name+b'\0');checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    p.run(0x4d0a30,args=(name,),callee_pop=False);maximum=sum(p.visits.values());out=p.reg('EAX')
    check(p.uint(f.teb)==0xffffffff,'balanced original SEH')
    raw=bytes(p.mu.mem_read(out,len(data)+4))
    check(raw==data+b'\0'*4,'whole bytes including embedded NUL and four appended zero bytes')
    check(not api.opened and p.visits.get(0x6bd710) and p.visits.get(0x6bdb60),'actual PC file open/read/destruction')
    check(p.visits.get(0x416d70) and p.visits.get(0x465540),'actual stream copy and ownership transfer')
    capacity=f.allocations[out];check(capacity==((len(data)+4+4999)//5000)*5000,'actual memory-stream growth quantum')
    p.run(0x412420,args=(out,),callee_pop=False)
    manager=p.uint(0x75db9c);cleanup(f,(manager,))
    check(set(f.allocations)==set(f.freed),'all file/stream/manager/result allocations freed')
    capture=[mode,raw.hex(),capacity,api.events]
    if not return_capture:print('PARSER_FILE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original parser file text {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
