#!/usr/bin/env python3
"""Bounded original461D40 math dependency; no generalized inverse assumption."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup,bits
def main():
    f=PCWriteBytesFixture();p=f.p;source=p.allocate(64);target=p.allocate(64)
    values=(1.,2.,3.,0.,4.,5.,6.,0.,7.,8.,10.,0.,11.,12.,13.,1.)
    for i,v in enumerate(values):p.put_uint(source+4*i,bits(v))
    f.call(0x461d40,this=source,args=(target,))
    print('INVERSE_SCOUT',list(p.floats(target,16)),'instructions',sum(p.visits.values()),flush=True)
    # First scout completed native math, but its diagnostic ALL-instruction
    # listing exceeded300 entries. Filter listing to x87 arithmetic instead;
    # this changes no execution/instruction/time/arena cap.
    from capstone import Cs,CS_ARCH_X86,CS_MODE_32
    disassembler=Cs(CS_ARCH_X86,CS_MODE_32);listing=[]
    for at in p.visits:
        ins=next(disassembler.disasm(bytes(p.mu.mem_read(at,16)),at),None)
        if ins and ins.mnemonic in ('fld','fmul','fadd','faddp','fsub','fsubp','fchs','fstp'):listing.append(f'{at:08X} {ins.mnemonic} {ins.op_str}')
    if len(listing)>300:raise AssertionError('bounded static x87 listing')
    for item in listing:print(item,flush=True)
    cleanup(f,());return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))
