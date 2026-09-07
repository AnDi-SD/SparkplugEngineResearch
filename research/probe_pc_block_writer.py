#!/usr/bin/env python3
"""Original PC nested block writer, bounded in-memory stream only.

No empty-stack native End, overflow error-manager path or allocator failure.
Known success/failure contracts and native mixed-width corruption are separate.
"""
from pathlib import Path
import json
import sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def header(identity,size,code):
    return bytes([(code<<5)|min(identity,31)])+(bytes([identity]) if identity>=31 else b'')+size.to_bytes({5:1,6:2,7:4}[code],'little')


class BlockFixture(PCWriteBytesFixture):
    def __init__(self):
        super().__init__();p=self.p;self.block=p.allocate(0x28)
        p.seams[0x4123f0]=self.allocate
        self.call(0x473000,this=self.block)
        check(self.call(0x472710,this=self.block,args=(self.stream,0x12345678))&255==1,'begin object')
        check(p.uint(self.block+0x1c)==0x12345678 and p.uint(self.block+0x20)==self.stream and not self.data,
              'begin only stores object/stream, no dereference/output')

    def begin(self,identity,code):return self.call(0x472d30,this=self.block,args=(identity,code))&255
    def end(self):return self.call(0x472e20,this=self.block,args=(99,))&255
    def depth(self):return self.p.uint(self.block+8)
    def payload(self,data):
        at=self.p.allocate(max(1,len(data)))
        if data:self.p.mu.mem_write(at,data)
        check(self.call(0x34000050,this=self.stream,args=(at,len(data)))&255==1,'fixture payload')

    def cleanup(self):
        # Original writer inlines list cleanup at43EC59..; this standalone
        # fixture explicitly frees remaining declared nodes, NOT dtor proof.
        for at in self.allocations:
            if at not in self.freed:self.p.run(0x412420,args=(at,),callee_pop=False)
        check(set(self.allocations)==set(self.freed),'bounded fixture allocations released')


def simple(f,identity,code,size):
    start=len(f.data);check(f.begin(identity,code)==1,'begin field')
    check(f.depth()==1,'one open header')
    check(f.data[start:]==header(identity,(1<<({5:8,6:16,7:32}[code]))-1,code),'all-one placeholder')
    payload=bytes(i%251 for i in range(size));f.payload(payload)
    check(f.end()==1 and f.depth()==0,'end ignores mismatching id99 and pops')
    expected=header(identity,size,code)+payload
    if f.data[start:]!=expected or f.position!=len(f.data):
        print('BLOCK_MISMATCH',identity,code,size,'actual',f.data[start:start+20].hex(),
              'expected',expected[:20].hex(),'position',f.position,'length',len(f.data),flush=True)
    check(f.data[start:]==expected and f.position==len(f.data),'forced width patch and position restore')
    return expected


def main(mode='success'):
    f=BlockFixture();outputs=[]
    if mode=='success':
        for code in (5,6,7):
            for identity,size in ((0,1),(3,8),(30,3),(255,255),(42,256 if code!=5 else 254)):
                outputs.append(simple(f,identity,code,size).hex())
        for code in (5,6,7):
            start=len(f.data);check(f.begin(42,code)==1,'outer begin');f.payload(b'A')
            check(f.begin(3,code)==1 and f.depth()==2,'nested begin');f.payload(b'bc')
            check(f.end()==1 and f.depth()==1,'nested end');f.payload(b'D')
            check(f.end()==1 and f.depth()==0,'outer end')
            inner=header(3,2,code)+b'bc';expected=header(42,len(inner)+2,code)+b'A'+inner+b'D'
            check(f.data[start:]==expected,'same-width nested patch')
            outputs.append(expected.hex())
        check(f.call(0x472b00,this=f.block,args=(0,))&255==1 and f.data[-1:]==b'\0','native final terminator')
        check(not f.errors,'successful writes no error diagnostics')
    elif mode=='zero-id31':
        for code in (5,6,7):
            start=len(f.data);check(f.begin(0,code)==1,'empty field begin')
            placeholder=f.data[start:];check(f.end()==1,'empty field end reports success')
            check(f.data[start:]==placeholder,'zero payload WriteHeader returns early; all-one size remains')
        start=len(f.data);check(f.begin(31,7)==1,'native ID31 begin')
        check(f.data[start:]==bytes.fromhex('ffffffffff'),
              'native treats ID31 as inline, omits required extended-ID byte')
        f.payload(b'abc');check(f.end()==1,'ID31 end')
        check(f.data[start:]==bytes.fromhex('ff03000000616263'),'ID31 exact malformed native output')
        check(not f.errors,'native zero/ID31 quirks silently report success')
    elif mode=='mixed':
        check(f.begin(42,7)==1,'outer begin');f.payload(b'A');check(f.begin(3,5)==1,'mixed begin');f.payload(b'bc')
        check(f.end()==1,'inner end');f.payload(b'D');check(f.end()==1,'outer end')
        check(f.data==bytes.fromhex('bf2a06ffffff41a302626344'),'native mixed nesting leaves three staleFF bytes')
        check(f.depth()==0 and f.p.uint(f.block+0x24)==5 and not f.errors,'global code persists without diagnostic')
    elif mode in {'fail-begin','fail-end'}:
        if mode=='fail-begin':
            f.fail_write_call=2
            check(f.begin(42,7)==0,'failed extended-ID write propagates false')
            check(f.data==b'\xff' and f.depth()==1 and f.position==1,'failed begin retains header and first byte')
        else:
            check(f.begin(42,7)==1,'begin before failed end');f.payload(b'abc');f.fail_write_call=f.write_calls+2
            check(f.end()==0,'failed patch write propagates false')
            check(f.data==bytes.fromhex('ff2affffffff616263') and f.depth()==1 and f.position==1,
                  'failed end retains header, stops at partial patch; no seek restore')
        check(len(f.errors)>=2,'native chained diagnostics')
    else:raise ValueError('unknown bounded block case')
    if outputs:print('BLOCK_OUTPUTS',json.dumps(outputs),flush=True)
    f.cleanup();print(f'PASS {checks}/{checks}: original PC block writer {mode}',flush=True)
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
