#!/usr/bin/env python3
"""Bounded original PC SAN object reader; byte stream and name registry fixtures.

The object directory/FAT loader is not executed. A verified single-object SAN
envelope supplies its unchanged spAnimation fields to the real native reader.
"""
from pathlib import Path
import math
import struct
import sys
import probe_pc_animation_lifecycle as lifetime
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture,check,x87_value
from probe_pc_animation_keys import install_acos_seams,cubic_coefficients,near,bits
from inspect_pc_san_keys import inspect,DEFAULT,u32,fields

def cstring(p,address):
    if not address:return b''
    result=bytearray()
    for i in range(4096):
        value=bytes(p.mu.mem_read(address+i,1))[0]
        if not value:return bytes(result)
        result.append(value)
    raise AssertionError('unterminated fixture name')

class ReaderFixture(LifetimeFixture):
    def __init__(self,data):
        super().__init__()
        self.data=data;self.position=0;self.names={};self.bound=[];self.unbound=[];self.errors=[]
        p=self.p
        self.stream=p.allocate(0x20);vt=p.allocate(0x40);p.put_uint(self.stream,vt)
        # Dedicated guest-only seam entry page: never overlap original code
        # (0x401000 is the real array-constructor helper used by this reader).
        p.mu.mem_map(0x34000000,0x1000,p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        for offset,address,callback in ((0x28,0x34000010,self.seek),(0x2c,0x34000020,self.tell),(0x30,0x34000030,self.read)):
            p.put_uint(vt+offset,address);p.seams[address]=callback
        p.seams[0x4130f0]=self.set_name
        p.seams[0x4173e0]=self.release_name
        p.seams[0x454370]=self.bind_name
        p.seams[0x453b10]=self.unbind_name
        p.seams[0x413540]=self.error_message
        install_acos_seams(p)

    def read(self,p):
        sp=p.reg('ESP');destination=p.uint(sp+4);count=p.uint(sp+8)
        if count>len(self.data)-self.position:
            p.fixture_return(8,eax=0);return
        if count:p.mu.mem_write(destination,self.data[self.position:self.position+count])
        self.position+=count
        p.fixture_return(8,eax=1)

    def tell(self,p):
        p.put_uint(p.uint(p.reg('ESP')+4),self.position)
        p.fixture_return(4,eax=1)

    def seek(self,p):
        sp=p.reg('ESP');mode=p.uint(sp+4);position=p.uint(sp+8)
        if mode!=1 or position>len(self.data):p.fixture_return(8,eax=0);return
        self.position=position;p.fixture_return(8,eax=1)

    def set_name(self,p):
        text=cstring(p,p.uint(p.reg('ESP')+4))
        entry=p.allocate(len(text)+10);self.allocations[entry]=len(text)+10
        p.mu.mem_write(entry+8,b'\x01'+text+b'\x00')
        p.put_uint(p.reg('ECX')+0x10,entry)
        p.fixture_return(4)

    def release_name(self,p):
        slot=p.reg('ECX');name=p.uint(slot)
        if name:
            check(name in self.allocations and name not in self.freed,'synthetic name ownership')
            self.freed.append(name);p.put_uint(slot,0)
        p.fixture_return()

    def bind_name(self,p):
        text=cstring(p,p.uint(p.reg('ESP')+4))
        if text not in self.names:self.names[text]=len(self.names)
        self.bound.append(text);p.fixture_return(4,eax=self.names[text])

    def unbind_name(self,p):
        self.unbound.append(cstring(p,p.uint(p.reg('ESP')+4)))
        p.fixture_return(4)

    def error_message(self,p):
        self.errors.append(cstring(p,p.uint(p.reg('ESP')+8)))
        p.fixture_return(eax=0) # variadic cdecl diagnostic, never UI/OS forwarding


def read_asset(name,*,capture_samples=False,quiet=False):
    path=DEFAULT/name
    summary,tracks=inspect(path)
    raw=path.read_bytes();start=u32(raw,20)
    f=ReaderFixture(raw[start+8:]);p=f.p
    animation=f.call(0x41a090)
    serializer=f.call(0x43dab0)
    check(f.allocations[serializer]==0x4c,'serializer exact factory size')
    result=f.call(0x43ecc0,this=serializer+0x10,args=(f.stream,animation))
    if not quiet:
        print('READER',name,'AL',result&255,'POSITION',f.position,'OF',len(f.data),
              'TRACKS',p.uint(animation+0x20),'CAPACITY',p.uint(animation+0x24),'ERRORS',f.errors,flush=True)
    check(result&255==1,'native reader succeeds')
    check(f.position==len(f.data) and not f.errors,'complete fields consumed without diagnostic')
    check(p.uint(animation+0x20)==len(tracks),'native track count matches readonly parser')
    expected_capacity=summary['track_reserve_hint']+1 if summary['track_reserve_hint'] is not None else len(tracks)
    check(p.uint(animation+0x24)==expected_capacity,'field64 reserves hint plus one; omission uses append growth')
    check(math.isclose(p.floats(animation+0x14,1)[0],summary['duration']),'native total time')
    captured={'time':summary['duration'],'capacity':expected_capacity,'tracks':[],
              'tags':[],'usedPools':summary['observed_pools']}
    working=p.allocate(0x80) if capture_samples else 0
    for i,expected in enumerate(tracks):
        track=p.uint(animation+0x1c)+i*0x44
        actual=cstring(p,p.uint(track+0x10)+9).decode('latin1')
        check(actual==expected['name'],'native track name matches wire')
        check(p.uint(track+0x14)==f.names[actual.encode('latin1')],'name binding slot fixture propagated')
        for role,record in expected['roles'].items():
            channels=record['channels']
            if not channels:
                check(p.uint(track+0x18+role*12)==0,'empty representation has no descriptor')
            for axis,channel in enumerate(channels):
                descriptor=p.uint(track+0x18+role*12+axis*4)
                count=channel['count'];rep=channel['representation']
                check(p.uint(descriptor)==count and p.uint(descriptor+4)==rep,'native descriptor metadata')
                check(not count or near(p.floats(p.uint(descriptor+8),count),channel['times']),'all native times match wire')
                values=channel['values']
                if rep==2 and role!=1:values=cubic_coefficients(values,3)
                if rep==4:values=cubic_coefficients(values,1)
                check(not values or near(p.floats(p.uint(descriptor+12),len(values)),values),'native prepared values match payload/formula')
        if capture_samples:
            p.mu.mem_write(working,bytes(0x80))
            f.call(0x478e30,this=track)
            item={'name':actual,'slot':p.uint(track+0x14),'duration':x87_value(p),'samples':[]}
            for factor in (0,.5,1):
                time=struct.unpack('<f',struct.pack('<f',summary['duration']*factor))[0]
                p.mu.mem_write(working,bytes(0x40))
                f.call(0x479290,this=track,args=(bits(time),working+0x40,working+0x4c,working+0x58,
                    working,working+0xc,working+0x1c,working+0x30,working+0x31,working+0x32))
                flags=list(bytes(p.mu.mem_read(working+0x30,3)))
                item['samples'].append([time,*flags,*p.floats(working,10)])
            captured['tracks'].append(item)
    # Tag name setter and normal destructor now execute original code, not a seam.
    expected_tags=[]
    for kind,payload,_ in fields(raw[start:],8):
        if kind==5:
            length=struct.unpack_from('<H',payload)[0]
            expected_tags.append((payload[2:2+length].rstrip(b'\0'),struct.unpack_from('<f',payload,2+length)[0],len(expected_tags)))
    expected_tags.sort(key=lambda value:value[1])
    captured['tags']=[[name.decode('latin1'),time,ordinal] for name,time,ordinal in expected_tags]
    tag_begin=p.uint(animation+0x2c);tag_end=p.uint(animation+0x30)
    check((tag_end-tag_begin)//4==len(expected_tags),'native tag count')
    for i,expected in enumerate(expected_tags):
        tag=p.uint(tag_begin+i*4)
        check((cstring(p,p.uint(tag+0x10)),p.floats(tag+0x14,1)[0],p.uint(tag+0x18))==expected,
              'native tag name/time/wire ordinal')
    f.call(0x430130,this=animation)
    check(f.bound==f.unbound,'all name bindings released in order')
    f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    return captured

def main(names):
    allowed={'bbush.san','bflower.san','barrel.san','bw.san'}
    if len(names)>2 or any(name not in allowed for name in names):raise ValueError('bounded pristine asset list only')
    for name in names or ['bbush.san']:read_asset(name)
    print(f'PASS {lifetime.checks}/{lifetime.checks}: full SAN object reader checks')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
