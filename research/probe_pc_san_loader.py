#!/usr/bin/env python3
"""Original complete PC generic SAN load body, declared startup/OS boundaries.

422B50 executes FFPS validation, resource/file indices, DX hook (no mesh),
RTTI lookup/factory, animation reader, real track-name registry and FAT clear.
Manager/FAT/RTTI initial containers are consumer-derived fixtures; no complete
startup, renderer, SMO mesh hook or native writer claim follows from this test.
"""
from collections import Counter
import hashlib
import math
from pathlib import Path
import struct
import sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture,empty_manager,empty_fat,animation_rtti
from pc_stl_fixtures import install_char_traits,read_cstring
from probe_pc_animation_manager import registry_entries
from probe_pc_animation_lifecycle import x87_value
from probe_pc_animation_keys import bits
from inspect_pc_san_keys import DEFAULT,inspect,u32,fields

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def capture(f,animation,summary,tracks,raw):
    p=f.p;working=p.allocate(0x80)
    output={'time':p.floats(animation+0x14,1)[0], 'capacity':p.uint(animation+0x24),
            'tracks':[], 'tags':[], 'usedPools':summary['observed_pools']}
    check(math.isclose(output['time'],summary['duration']),'native duration from original envelope')
    check(p.uint(animation+0x20)==len(tracks),'native full-loader track count')
    expected_capacity=summary['track_reserve_hint']+1 if summary['track_reserve_hint'] is not None else len(tracks)
    check(output['capacity']==expected_capacity,'reserve hint and append growth unchanged')
    for index,expected in enumerate(tracks):
        track=p.uint(animation+0x1c)+index*0x44
        name=read_cstring(p,p.uint(track+0x10)+9).decode('latin1')
        check(name==expected['name'],'native track name from whole load')
        f.call(0x478e30,this=track)
        item={'name':name,'slot':p.uint(track+0x14),'duration':x87_value(p),'samples':[]}
        for factor in (0,.5,1):
            time=struct.unpack('<f',struct.pack('<f',output['time']*factor))[0]
            p.mu.mem_write(working,bytes(0x80))
            f.call(0x479290,this=track,args=(bits(time),working+0x40,working+0x4c,working+0x58,
                working,working+0xc,working+0x1c,working+0x30,working+0x31,working+0x32))
            item['samples'].append([time,*bytes(p.mu.mem_read(working+0x30,3)),*p.floats(working,10)])
        output['tracks'].append(item)
    expected_tags=[]
    for kind,payload,_ in fields(raw[u32(raw,20):],8):
        if kind==5:
            length=struct.unpack_from('<H',payload)[0]
            expected_tags.append((payload[2:2+length].rstrip(b'\0').decode('latin1'),
                                  struct.unpack_from('<f',payload,2+length)[0],len(expected_tags)))
    begin,end=p.uint(animation+0x2c),p.uint(animation+0x30)
    for i in range((end-begin)//4):
        tag=p.uint(begin+i*4)
        output['tags'].append([read_cstring(p,p.uint(tag+0x10)).decode('latin1'),
                               p.floats(tag+0x14,1)[0],p.uint(tag+0x18)])
    check(output['tags']==[list(t) for t in sorted(expected_tags,key=lambda t:t[1])],
          'native tags keep wire ordinal and runtime sorted time')
    return output


def load_asset(name,*,quiet=False,reuse=True):
    # Cold whole-load scouts for bflower/barrel/bw reached100k at472923,
    # 41FB28 and13D7892 respectively. Do not repeat/resume those bounded stops.
    if name!='bbush.san':raise ValueError('only whole-load proven bbush; other assets remain staged reader evidence')
    summary,tracks=inspect(DEFAULT/name);raw=(DEFAULT/name).read_bytes()
    f=PCFileBytesFixture(raw);p=f.p
    animation_rtti(f);install_char_traits(p)
    del p.seams[0x454370];del p.seams[0x453b10]
    names_manager=f.call(0x454640)
    fat=empty_fat(f);manager,_=empty_manager(f)
    p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x43dab0)
    f.call(0x422d90,this=manager,args=(0x56ee563a,serializer,0xffffffff,1))
    names=[t['name'].encode('latin1') for t in tracks];refs=Counter(names)
    slots={name:i+1 for i,name in enumerate(dict.fromkeys(names))}
    objects=[];captured=None;instruction_counts=[]
    for ordinal in range(2 if reuse else 1):
        # A fresh file-open state is explicit: native loader advances origin;
        # seeking to physical zero without resetting it is not a valid reopen.
        f.position=0;p.put_uint(f.stream+0x14,0);f.io.clear()
        animation=f.call(0x422b50,this=manager,args=(f.stream,))
        instruction_counts.append(sum(p.visits.values()));visited=set(p.visits)
        check(animation in f.allocations and p.uint(animation)==0x6de6cc,'actual RTTI animation factory result')
        for entry in (0x422260,0x466b90,0x465cd0,0x4aa430,0x4aab80,0x422940,
                      0x4143f0,0x414420,0x467550,0x43ecc0,0x466760,0x466870):
            check(entry in visited,f'original chain actually visited {entry:08X}')
        check(f.position==len(raw) and not f.errors,'all SAN bytes consumed without error')
        check(p.uint(f.stream+0x14)==u32(raw,20),'loader publishes data origin without restoring it')
        check(all(op[2]==op[3] for op in f.io if op[0]=='read'),'valid asset never relies on short-read success')
        check(p.uint(fat+0x28)==p.uint(fat+0x34)==p.uint(fat+0x50)==0 and
              p.uint(fat+0x10)==1,'loader clears resource maps/list and resets next ID')
        check(f.call(0x408370,this=animation,args=(0x44de07fd,))&255==0,
              'engine RTTI does not make Animation a NamedObject despite physical named prefix')
        check(registry_entries(p,names_manager)=={n:(slots[n],c*(ordinal+1)) for n,c in refs.items()},
              'simultaneous full loads share actual original name registry')
        if captured is None:captured=capture(f,animation,summary,tracks,raw)
        objects.append(animation)
    for remaining,animation in zip(range(len(objects)-1,-1,-1),objects):
        f.call(p.uint(p.uint(animation)),this=animation,args=(1,))
        check(registry_entries(p,names_manager)==({n:(slots[n],c*remaining) for n,c in refs.items()} if remaining else {}),
              'full-loaded animation releases only its own name references')
    f.call(0x4228a0,this=manager)
    for global_address in (0x75db78,0x75526c):
        obj=p.uint(global_address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    f.call(0x4545d0,this=names_manager,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native resource/hook/FAT/registry allocations freed exactly once')
    if not quiet:
        print('ASSET',name,'SHA256',hashlib.sha256(raw).hexdigest().upper(),
              'full-load instructions',instruction_counts,'tracks',len(tracks),flush=True)
    return captured


def main(names):
    if len(names)>1:raise ValueError('one asset per30s child')
    load_asset(names[0] if names else 'bbush.san')
    print(f'PASS {checks}/{checks}: original complete PC SAN loader contracts')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
