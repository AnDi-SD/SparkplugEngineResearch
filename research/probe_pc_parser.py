#!/usr/bin/env python3
"""Actual PC parser factory and text normalization in bounded guest memory."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import install_char_traits
from probe_pc_function_eval import cleanup

CASES={
    'empty':b'',
    'words':b'  alpha \t beta\r\n gamma  ',
    'operators':b'a + b = ( c , d ) ; x / y : z',
    'line-comment':b'a // gone\n b // second\r\nc',
    'block-comment':b'a /* gone */ b/**/c /* x\ny */ d',
    'quoted':b'  "a  // b /* c */" \t "x\ny"  ',
    'controls':b'a\x01b\x08c\x0bd\x0ce\x1ff',
    'newlines':b'a\nb\rc\r\nd\t e',
    'escaped-quote':b'"a\\" /* b */ "c"',
}

def main(mode,return_capture=False):
    if mode not in (*CASES,'parser-factory','rfx-factory','clone','rfx-clone'):
        raise ValueError('bounded parser case')
    f=PCWriteBytesFixture();p=f.p;install_char_traits(p);checks=0;maximum=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    rfx=mode in ('rfx-factory','rfx-clone');obj=call(0x4d54b0 if rfx else 0x4d0da0);owned=[obj]
    check(f.allocations[obj]==(0x728 if rfx else 0x538),'actual factory size')
    check(p.uint(obj)==(0x6f3da4 if rfx else 0x6f3a94),'actual parser/loader vtable')
    delimiters=[i for i in range(255) if bytes(p.mu.mem_read(obj+0x24+i,1))[0]]
    check(delimiters==[9,10,13,32,33,40,41,42,43,44,45,47,58,59,60,61,62,63,91,93,94,123,125,126],'actual delimiter lookup table')
    check(all(p.uint(obj+i)==0 for i in (0x14,0x18,0x1c,0x20,0x128,0x12c,0x130)),'empty text and token storage')
    check(p.uint(obj+0x120)==0xcc000000 and p.uint(obj+0x124)==0xcccccccc,'lookup tail/padding untouched')
    states=[]
    if mode in CASES:
        data=CASES[mode];source=p.allocate(len(data)+16)
        p.mu.mem_write(source,data+b'\0'+b'\xfe'*15)
        # Length is strlen: input end points to the supplied NUL. This call
        # allocates exactly length bytes yet may write length+1; observe it in
        # the aligned synthetic arena without claiming host memory safety.
        result=call(0x4d0bf0,this=obj,args=(source,len(data)))&255
        output=p.uint(obj+0x14);size=p.uint(obj+0x1c)
        check(result==1 and bytes(p.mu.mem_read(obj+0x10,1))==b'\1','normalization transfers ownership to new allocation')
        check(size<=len(data)+1 and p.uint(obj+0x18)==source+len(data),'stale end pointer is not recomputed')
        check(p.uint(obj+0x20)==1,'line/current state initialized to1')
        check(f.allocations[output]==len(data),'native output allocation is input length')
        raw=bytes(p.mu.mem_read(output,size));check(raw[-1:]==b'\0','appended output terminator')
        states=[data.hex(),raw.hex(),size,f.allocations[output],size>f.allocations[output]]
    elif mode in ('clone','rfx-clone'):
        call(0x52fd90,this=0x755588)
        p.put_uint(obj+0x20,99);clone=call(0x4d5510 if rfx else 0x4d0e00,this=obj);owned.insert(0,clone)
        check(clone!=obj and p.uint(clone+0x20)==0 and p.uint(clone+0x14)==0,'clone is a new empty parser')
        call(0x6d7db0);states=[p.uint(obj+0x20),p.uint(clone+0x20)]
    cleanup(f,owned);check(set(f.allocations)==set(f.freed),'owned allocations all freed by original destructors')
    capture=[mode,delimiters,states]
    if not return_capture:print('PARSER_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original parser {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
