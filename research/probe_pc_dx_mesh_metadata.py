#!/usr/bin/env python3
"""Directed original4AA4E0 metadata consumer; bounded guest, no GPU/OS forwarding."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

checks=0
def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)

HEADER=struct.pack('<II',0x33c34cf0,0x4f4f4253)
VALUES=(0x112,123,3936,738,1)
def field(values=VALUES):return b'\xa1\x11'+struct.pack('<IIIIB',*values)

class Fixture(PCFileBytesFixture):
    def __init__(self,data,mode):
        self.mode=mode;self.read_count=0;self.seek_count=0
        super().__init__(data)
    def read(self,p):
        self.read_count+=1
        if self.mode.startswith('fail-read-') and self.read_count==int(self.mode.rsplit('-',1)[1]):
            p.fixture_return(8,eax=0);return
        super().read(p)
    def tell(self,p):
        if self.mode=='fail-tell':p.fixture_return(4,eax=0);return
        super().tell(p)
    def seek(self,p):
        self.seek_count+=1
        if self.mode=='fail-rewind' and self.seek_count==2:
            p.fixture_return(8,eax=0);return
        super().seek(p)

def main(mode):
    allowed={'normal','bad-header','no-field','missing-terminator','empty-fields',
             'duplicate','unknown','bad-offset','fail-tell','fail-rewind','byte-bool'}
    if mode not in allowed and mode not in {f'fail-read-{i}' for i in range(1,9)}:
        raise ValueError('explicit bounded metadata mode required')
    body=field()+b'\0';expected=VALUES
    header=HEADER
    if mode=='bad-header':header=struct.pack('<II',0xdeadbeef,0x12345678)
    if mode=='no-field':body=b'\0';expected=(0xcdcdcdcd,)*4+(0xcd,)
    if mode=='empty-fields':body=b'';expected=(0xcdcdcdcd,)*4+(0xcd,)
    if mode=='missing-terminator':body=field()
    if mode=='unknown':body=b'\x22X'+field()+b'\x43YZ\0'
    if mode=='duplicate':
        expected=(0x142,7,224,42,0);body=field()+field(expected)+b'\0'
    if mode=='byte-bool':expected=(*VALUES[:4],0xa5);body=field(expected)+b'\0'
    f=Fixture(header+body,mode);p=f.p
    entry=p.allocate(0x24);p.put_uint(entry+0x14,999 if mode=='bad-offset' else 0)
    output=p.allocate(20);p.mu.mem_write(output,b'\xcd'*20)
    result=f.call(0x4aa4e0,args=(f.stream,entry,output,output+4,output+8,output+12,output+16))&255
    actual=tuple(p.uint(output+i) for i in (0,4,8,12))+(bytes(p.mu.mem_read(output+16,1))[0],)
    print('METADATA',mode,'AL',result,'values',actual,'position',f.position,
          'readCalls',f.read_count,'instructions',sum(p.visits.values()),'errors',f.errors,flush=True)
    if mode in {'normal','bad-header','no-field','empty-fields','missing-terminator','duplicate','unknown','byte-bool'}:
        check(result==1,'native metadata success')
        check(actual==expected,'five output values, including unchanged output when no native field')
        check(f.position==len(f.data),'full supplied bytes consumed')
        if mode in {'empty-fields','missing-terminator'}:
            check(bool(f.errors),'ReadHeader failure reports diagnostic yet metadata helper returns true')
        else:check(not f.errors,'valid scan has no diagnostic')
    elif mode in {'fail-read-2','fail-read-3'}:
        check(result==1 and actual==(0xcdcdcdcd,)*4+(0xcd,),
              'ReadHeader false/null becomes metadata success with unchanged output')
        check(bool(f.errors),'failed compact header has native diagnostic')
    else:
        check(result==0,'explicit read/seek/tell failure returns false')
        check(bool(f.errors),'failure has native diagnostic')
    check(set(f.allocations)==set(f.freed),'data-block temporary allocations freed')
    check(0x4a9610 not in p.visits and 0x4a96c0 not in p.visits,'no combiner/buffer/GPU construction')
    print(f'PASS {checks}/{checks}: original DX mesh metadata {mode}')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
