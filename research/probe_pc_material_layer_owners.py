#!/usr/bin/env python3
"""Actual direct ownership and NULL slot semantics, not intrusive pass semantics."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

def main():
    checks=0
    def check(ok,text):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(text)
    f=PCFileBytesFixture(b'');p=f.p;owner=f.call(0x45f610);layer=f.call(0x460e50);texture=p.uint(layer+0x10)
    check(f.allocations[owner]==0x38 and f.allocations[layer]==0x14 and f.allocations[texture]==0x68,'actual pass/layer/texture sizes')
    check(f.call(0x45f5e0,this=owner,args=(2,layer))&255==1 and p.uint(owner+0x14)==3,'native sparse layer slot2')
    check(p.uint(layer+8)&65535==0 and p.uint(texture+8)&65535==0,'direct ownership does not retain either nested object')
    check(f.call(0x45f5e0,this=owner,args=(2,0))&255==1 and layer in f.freed and texture in f.freed,'NULL replacement directly deletes whole nested layer')
    check(p.uint(owner+0x14)==3 and p.uint(owner+0x20)==0,'NULL layer does not shrink or shift slots')
    check(f.call(0x45f5e0,this=owner,args=(7,0))&255==1 and p.uint(owner+0x14)==8,'NULL slot7 still grows count8')
    f.call(p.uint(p.uint(owner)),this=owner,args=(1,))
    for address in (0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all native direct-owner objects freed')
    print(f'PASS {checks}/{checks}: pass direct-owned layer slots, NULL growth and nested destruction')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
