#!/usr/bin/env python3
"""Actual Node billboard setter/field branch, stop before camera world consumer."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture
from probe_pc_node_serializer import field,check
import probe_pc_node_serializer as counters

def main():
    for value,mask in ((0,0),(1,0x100000),(2,0x200000),(3,0),(0xffffffff,0)):
        f=PCFileBytesFixture(field(6,struct.pack('<I',value))+b'\0');p=f.p;f.call(0x6d38e0)
        node=f.call(0x421e20);serializer=f.call(0x4638f0);p.put_uint(node+0xb0,0x370a00)
        p.run(0x463a70,this=serializer+0x10,args=(f.stream,node),stop_at=0x463c95)
        check(p.uint(node+0xb0)==0x70a00|mask,'billboard value selects only1/2, clears both bits for other values')
        check(f.position==len(f.data) and 0x463820 in p.visits,'actual complete field parsing before world callback')
        # Explicit stopped frame: camera world consumer NOT executed/resumed.
        head=p.uint(p.reg('ESP')+0x44)
        check(f.allocations.get(head)==24 and p.uint(head)==head,'original stopped DataBlock head')
        p.put_uint(f.teb,0xffffffff);p.run(0x412420,args=(head,),callee_pop=False)
        f.call(0x422220,this=node,args=(1,));f.call(0x4639b0,this=serializer,args=(1,))
        check(set(f.allocations)==set(f.freed),'all explicit aborted-frame allocations cleaned')
    print(f'PASS {counters.checks}/{counters.checks}: original Node billboard fields; camera world path not claimed')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
