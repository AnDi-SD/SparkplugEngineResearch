#!/usr/bin/env python3
"""Original PC save index/reference protocol, no whole FFPS save claim."""
from pathlib import Path
import struct
import sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import animation_rtti,empty_fat,empty_manager
from probe_pc_san_reader import cstring

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


class SaveFixture(PCWriteBytesFixture):
    def __init__(self):
        self.seek_calls=0;self.fail_seek_call=None
        super().__init__();p=self.p;animation_rtti(self)
        self.fat=empty_fat(self);self.manager,_=empty_manager(self)
        p.put_uint(self.manager+0x28,self.fat);p.put_uint(self.manager+0x10,1)
        p.put_uint(self.manager+0x14,2);p.put_uint(self.manager+0x18,2);p.put_uint(0x75dde8,self.manager)
        self.animation=self.call(0x41a090);self.serializer=self.call(0x43dab0)
        self.call(0x422d90,this=self.manager,args=(0x56ee563a,self.serializer,0xff,3))

    def seek(self,p):
        self.seek_calls+=1
        if self.seek_calls==self.fail_seek_call:
            self.io.append(('seek-failed',self.position));p.fixture_return(8,eax=0);return
        super().seek(p)

    def index(self):return self.call(0x4672c0,this=self.serializer,args=(self.animation,))&255
    def entry(self):return self.call(0x4664f0,this=self.fat,args=(self.animation,))
    def reference(self,object_pointer=None):
        return self.call(0x467350,this=self.serializer,args=(self.stream,self.animation if object_pointer is None else object_pointer))&255
    def cleanup(self):
        p=self.p
        self.call(0x466760,this=self.fat)
        self.call(p.uint(p.uint(self.animation)),this=self.animation,args=(1,))
        self.call(0x4228a0,this=self.manager)
        for global_at in (0x75526c,0x755264):
            obj=p.uint(global_at)
            if obj:self.call(p.uint(p.uint(obj)),this=obj,args=(1,))
        check(set(self.allocations)==set(self.freed),'all native allocations released; fixture heads not counted as native')


def index_case():
    f=SaveFixture();p=f.p
    check(f.entry()==0,'fresh object absent')
    check(f.index()==1,'serializer wrapper indexes new object')
    visited=set(p.visits)
    check(0x466fa0 in visited and 0x5a7db0 in visited,'actual FAT and relationship slot invoked')
    entry=f.entry()
    check(f.allocations[entry]==0x24 and p.uint(entry)==0x6e7f3c,'native save entry24')
    check(f.words(entry,4,0x24)==[1,0,0,0x56ee563a,0,0,0xcccccc00,f.animation],
          'save constructor initializes only flag byte, null name and zero offsets')
    check(p.uint(f.fat+0x10)==2 and p.uint(f.fat+0x28)==p.uint(f.fat+0x34)==p.uint(f.fat+0x50)==1,
          'save increments ID and populates both maps and ordered list')
    check(p.uint(0x13b1b78)==0x47dd80,'native protected save constructor resolves')
    check(f.call(0x466fa0,this=f.fat,args=(0x56ee563a,f.animation))&255==0,'direct duplicate rejects')
    check(f.index()==1 and 0x5a7db0 not in p.visits,'wrapper duplicate becomes success and skips relationships')
    check(p.uint(f.fat+0x10)==2 and f.entry()==entry,'duplicate consumes no ID and preserves entry')
    check(f.call(0x467300,this=f.serializer,args=(0,))&255==1,'native null index-reference succeeds after error gate')
    check(f.call(0x467300,this=f.serializer,args=(f.animation,))&255==1,'native index-reference dispatch succeeds')
    check(0x422530 in p.visits and 0x4672c0 in p.visits and 0x5a7db0 not in p.visits,
          'index-reference uses actual class lookup and skips duplicate relationship recursion')
    # Physical name is not an engine NamedObject relationship for Animation.
    name=p.allocate(8);p.mu.mem_write(name,b'named\0')
    f.call(0x4130f0,this=f.animation,args=(name,));f.call(0x466760,this=f.fat)
    check(f.index()==1 and p.uint(f.entry()+0xc)==0,'Animation engine RTTI suppresses physical name during index')
    check(not f.errors,'normal indexing no diagnostics');f.cleanup()


def entry_names():
    f=PCWriteBytesFixture();p=f.p;p.seams[0x4123f0]=f.allocate
    for text in (None,b'',b'name',b'path/to/mesh'):
        entry=p.allocate(0x24);p.mu.mem_write(entry,b'\xcc'*0x24);name=0
        if text is not None:
            name=p.allocate(len(text)+1);p.mu.mem_write(name,text+b'\0')
        check(f.call(0x465bf0,this=entry,args=(7,3,name,0x56ee563a,0x12345678))==entry,'entry constructor return')
        check(f.words(entry,4,0xc)==[7,3] and f.words(entry,0x10,0x24)==[0x56ee563a,0,0,0xcccccc00,0x12345678],
              'constructor field values and untouched flag padding')
        owned=p.uint(entry+0xc)
        check(bool(owned)==bool(text),'null and empty input names normalize to null pointer')
        if text:
            check(owned!=name and cstring(p,owned)==text and f.allocations[owned]==len(text)+1,
                  'nonempty name deep copy incl terminator')
        f.call(0x465ca0,this=entry,args=(0,))
    check(set(f.allocations)==set(f.freed),'entry names released, borrowed object not dereferenced')


def reference_case(mode):
    f=SaveFixture();p=f.p;check(f.index()==1,'index resource before reference');entry=f.entry()
    if mode=='null':
        check(f.reference(0)==1 and f.data==bytes(4),'null reference exactly four zero bytes')
        check(p.uint(entry+0x1c)==0xcccccc00,'null reference does not mark other payload');f.cleanup();return
    if mode=='fail-id':f.fail_write_call=1
    if mode=='fail-size':f.fail_write_call=2
    # Writer patches value pools, then time pool, then restores output end.
    if mode=='fail-payload-final-seek':f.fail_seek_call=3
    if mode=='fail-patch-seek':f.fail_seek_call=4
    if mode=='fail-restore-seek':f.fail_seek_call=5
    if mode=='fail-patch-write':f.fail_write_call=38
    result=f.reference();visited=set(p.visits)
    if mode in {'fail-id','fail-size'}:
        check(result==0 and f.write_calls==(1 if mode=='fail-id' else 2),'early reference write failure propagates')
        check(f.data==(b'' if mode=='fail-id' else struct.pack('<I',1)),'early partial bytes remain')
        check(p.uint(entry+0x1c)==0xcccccc00 and f.words(entry,0x14,0x1c)==[0,0],'failure before payload flag leaves entry unwritten')
        check(bool(f.errors),'early write diagnostic');f.cleanup();return
    expected_fields=bytes.fromhex('60000000007f40000000006c0000000066000000006700000000680000000069000000006a000000006b0000000000')
    expected=struct.pack('<IIII',1,55,0x56ee563a,0x4f4f4253)+expected_fields
    check(result==1,'native reference reports success')
    check(f.words(entry,0x14,0x24)==[8,25 if mode=='fail-payload-final-seek' else 55,0xcccccc01,f.animation],
          'entry offset starts at SBOO header; size uses actual cursor; flag byte set')
    check(0x467260 in visited and 0x43dfe0 in visited and p.uint(p.uint(p.uint(0x755264))+0x20) in visited,
          'actual header, payload writer and ErrorManager gate used')
    if mode=='success':
        check(f.data==expected and f.position==63 and f.write_calls==38,'exact63-byte inline reference and call count')
        p.put_floats(f.animation+0x14,[5])
        check(f.reference()==1 and 0x43dfe0 not in p.visits,'repeat reference skips modified payload entirely')
        check(f.data==expected+struct.pack('<II',1,0),'repeat ID plus zero size; no duplication')
        check(f.words(entry,0x14,0x1c)==[8,55],'repeat retains original resource extent')
        check(f.reference(0)==1 and f.data==expected+struct.pack('<III',1,0,0),'null appended after repeated reference')
        print('REFERENCE_OUTPUT_HEX',f.data.hex(),flush=True)
    elif mode=='fail-payload-final-seek':
        check(f.data==expected[:4]+struct.pack('<I',25)+expected[8:32]+b'\0'+expected[33:-1] and f.position==33,
              'unchecked payload-final seek puts terminator inside pool header; outer extent becomes25')
    elif mode=='fail-patch-seek':
        check(f.data==expected[:4]+bytes(4)+expected[8:]+struct.pack('<I',55) and f.position==63,
              'unchecked failed patch seek appends size, original placeholder remains zero')
    elif mode=='fail-restore-seek':
        check(f.data==expected and f.position==8,'unchecked failed final seek leaves cursor in header')
    elif mode=='fail-patch-write':
        check(f.data==expected[:4]+bytes(4)+expected[8:] and f.position==63,
              'unchecked failed final size write reports success with zero placeholder')
    else:raise ValueError('unsupported reference mode')
    check(not f.errors,'native ignores final patch failures without diagnostic');f.cleanup()


def main(mode):
    if mode=='index':index_case()
    elif mode=='entry-names':entry_names()
    else:reference_case(mode)
    print(f'PASS {checks}/{checks}: original PC save-reference {mode}')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
