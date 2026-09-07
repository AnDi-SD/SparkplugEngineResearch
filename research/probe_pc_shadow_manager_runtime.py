#!/usr/bin/env python3
"""Actual DXShadow ctor/clone/destructor/RTTI; no shader or shadow mesh claim."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_static_render_runtime import StaticFixture

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def main():
    f=StaticFixture()
    p=f.p
    for record,identity,parent in ((0x75ac00,0x20a72504,0x7555f8),
                                    (0x763960,0x63fea321,0x75ac00),
                                    (0x763028,0x04680bc1,0x763960)):
        p.put_uint(record,identity)
        p.put_uint(record+0x48,parent)
    obj=f.object(0x4a9030)
    check(f.allocations[obj]==0x3c and p.uint(obj)==0x6ef138 and
          p.uint(obj+0x14)==0x6ef134,'exact3C primary/support tables')
    check(p.uint(0x75db7c)==obj and p.uint(obj+0x10)==0,'ctor publishes singleton, empty inherited name')
    check(bytes(p.mu.mem_read(obj+0x18,4))==b'\1\0\xcc\xcc',
          'Enabled18 true, optional19 false, padding untouched')
    check(f.words(obj,0x1c,0x30)==[0]*5 and p.uint(obj+0x38)==1 and
          p.uint(obj+0x30)==p.uint(obj+0x34)==0xcccccccc,'known zeros/default1 and uninitialized shader handles')
    for identity in (0x63fea321,0x20a72504,0x44de07fd,0x415352a1):
        check(f.call(0x408370,this=obj,args=(identity,))&255==1,'original transitive RTTI '+hex(identity))
    check(f.call(0x408350,this=obj,args=(0x04680bc1,))&255==1 and
          f.call(0x408350,this=obj,args=(0x63fea321,))&255==0,'concrete vs abstract identity')
    p.mu.mem_write(obj+0x18,b'\0\1')
    for offset in range(0x1c,0x3c,4):p.put_uint(obj+offset,0x12340000+offset)
    clone=f.call(0x4a90b0,this=obj)
    f.objects.append(clone)
    check(clone!=obj and f.allocations[clone]==0x3c and p.uint(0x75db7c)==clone,
          'actual clone creates fresh manager and ctor replaces global singleton')
    check(bytes(p.mu.mem_read(clone+0x18,4))==b'\1\0\xcc\xcc' and
          f.words(clone,0x1c,0x30)==[0]*5 and p.uint(clone+0x38)==1,
          'inherited-only clone does not copy source runtime controls/pointers')
    check(p.uint(clone+0x30)==p.uint(clone+0x34)==0xcccccccc,
          'uninitialized shader handles remain original constructor bytes, not source copies')
    # Deleting any manager clears singleton, even while another exists.
    f.call(0x4a7c70,this=obj+0x14,args=(1,))
    f.objects.remove(obj)
    check(obj in f.freed and p.uint(0x75db7c)==0 and clone not in f.freed,
          'secondary deleting adapter adjusts-14; destructor clears singleton unconditionally')
    f.close()
    check(clone in f.freed,'all owned native objects and clone-map storage released')
    print(f'PASS {checks}/{checks}: original DXShadowManager lifetime; nonempty shader path open')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
