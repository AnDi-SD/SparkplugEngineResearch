#!/usr/bin/env python3
"""Bounded original PC FAT resource index/cursor/clear and RTTI consumers.

Explicit valid container/RTTI input, allocation/read/diagnostic fixtures.
Complete constructor, startup registration and rollback are not substituted.
"""
from pathlib import Path
import struct
import sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import empty_fat,animation_rtti
from probe_pc_san_reader import ReaderFixture,cstring

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def rtti():
    f=ReaderFixture(b'');p=f.p;manager=animation_rtti(f)
    for identity,expected in ((0,0),(0x56ee563a,1),(0xffffffff,0)):
        check(f.call(0x4143f0,this=manager,args=(identity,))&255==expected,'native RTTI membership on explicit tree')
        check(p.uint(0x13b2d14)==0x4423f0,'protected membership bridge resolves independently')
    check(f.call(0x414420,this=manager,args=(0,))==0,'missing class has no factory result')
    p.put_uint(0x75d248+0x4c,0)
    check(f.call(0x414420,this=manager,args=(0x56ee563a,))==0,'abstract/null factory returns null')
    p.put_uint(0x75d248+0x4c,0x41a090)
    animation=f.call(0x414420,this=manager,args=(0x56ee563a,))
    check(f.allocations[animation]==0x84 and p.uint(animation)==0x6de6cc,'actual registered animation factory')
    f.call(p.uint(p.uint(animation)),this=animation,args=(1,))
    debug=p.uint(0x75526c)
    if debug:f.call(p.uint(p.uint(debug)),this=debug,args=(1,))
    check(set(f.allocations)==set(f.freed),'all factory-created allocations released')


def ordered_entries(f,fat):
    p=f.p;entries=[];entry=f.call(0x465f00,this=fat)
    while entry:
        check(len(entries)<16,'bounded resource cursor')
        entries.append(entry);entry=f.call(0x465f20,this=fat)
    check(p.uint(fat+0x54)==p.uint(fat+0x4c),'cursor reaches allocated sentinel')
    return entries


def index_case(ids):
    data=struct.pack('<I',len(ids))
    for i,identity in enumerate(ids):
        name=f'item{i}'.encode()+b'\0'
        data+=struct.pack('<IH',identity,len(name))+name+struct.pack('<III',0x56ee563a,10+i,100+i)
    f=ReaderFixture(data);p=f.p;animation_rtti(f);fat=empty_fat(f)
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'original index reads registered classes')
    check(f.position==len(data) and not f.errors,'exact index bytes consumed')
    entries=ordered_entries(f,fat)
    check([p.uint(e+4) for e in entries]==ids,'cursor preserves insertion order including duplicates')
    check(p.uint(fat+0x28)==len(set(ids)) and p.uint(fat+0x50)==len(ids),
          'native duplicate overwrites map but still appends separate ordered entry')
    for i,entry in enumerate(entries):
        check(f.allocations[entry]==0x24 and p.uint(entry)==0x6e7f3c,'native24 resource entry allocation and vtable')
        check(cstring(p,p.uint(entry+0xc))==f'item{i}'.encode(),'entry owns exact name')
        check(f.words(entry,0x10,0x1c)==[0x56ee563a,10+i,100+i],'class/offset/size fields intact')
        check(p.uint(entry+8)==p.uint(entry+0x20)==0 and p.uint(entry+0x1c)==0xcccccccc,
              'fileID/object default zero, payloadWritten storage left uninitialized')
    check(p.uint(fat+0x10)==1 and p.uint(fat+0x34)==0,'load does not advance writer ID or populate object map')
    # Attach borrowed allocated objects: FAT clears entries, not runtime objects.
    borrowed=[f.call(0x41a090) for _ in entries]
    for entry,obj in zip(entries,borrowed):p.put_uint(entry+0x20,obj)
    f.call(0x466760,this=fat)
    latest={identity:entry for identity,entry in zip(ids,entries)}
    check([entry for entry in f.freed if entry in entries]==[latest[i] for i in sorted(latest)],
          'clear destroys map-reachable entries in unsigned ID order')
    check(all(obj not in f.freed for obj in borrowed),'resource index clear does not own materialized objects')
    check(p.uint(fat+0x28)==p.uint(fat+0x34)==p.uint(fat+0x50)==0 and p.uint(fat+0x10)==1,
          'native clear resets all three resource containers and next ID')
    # Clear leaves cursor stale; FirstEntry safely refreshes it before use.
    check(ordered_entries(f,fat)==[],'first cursor after clear is safe and empty')
    f.call(0x466760,this=fat)
    # Orphaned duplicate entries are a native leak. Explicitly call their real
    # destructors for fixture teardown, not a claim that clear found them.
    for entry in entries:
        if entry not in f.freed:f.call(0x465ca0,this=entry,args=(1,))
    for obj in borrowed:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    debug=p.uint(0x75526c)
    if debug:f.call(p.uint(p.uint(debug)),this=debug,args=(1,))
    check(set(f.allocations)==set(f.freed),'explicit teardown releases native allocations including known duplicate orphans')


def main(mode):
    if mode=='rtti':rtti()
    elif mode=='index':
        index_case([]);index_case([7,3,0xffffffff]);index_case([7,7])
    else:raise ValueError('bounded profile required')
    print(f'PASS {checks}/{checks}: original PC FAT {mode} contracts')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
