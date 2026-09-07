#!/usr/bin/env python3
"""Single bounded dirty-matrix consumer scout, original protected4AD540."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup
def main():
    f=ApplyFixture(renderer_size=0xf2f8);p=f.p;r=f.renderer
    for ordinal,offset in enumerate((0xca40,0xca80,0xcac0),1):
        matrix=[0.]*16
        for i,value in enumerate((float(ordinal),float(ordinal+1),float(ordinal+2),1.)):matrix[5*i]=value
        matrix[12:15]=[float(ordinal*2),float(ordinal*3),float(ordinal*4)];p.put_floats(r+offset,matrix)
    p.mu.mem_write(r+0xf2f4,b'\1');f.call(0x4ad540,this=r);maximum=sum(p.visits.values())
    print('MATRIX_SCOUT',json.dumps({'matrices':[[hex(off),list(p.floats(r+off,16))] for off in range(0xca40,0xcbc0,64)],'dirty':int.from_bytes(p.mu.mem_read(r+0xf2f4,1),'little')}),flush=True)
    code=[]
    for address in p.visits:
        ins=next(p.decoder.disasm(bytes(p.mu.mem_read(address,15)),address),None)
        if ins and (0x13d6a00<=address<0x13d6b40 or 0x440000<=address<0x600000):code.append(f'{address:08X} {ins.mnemonic} {ins.op_str}')
    if len(code)>800:raise AssertionError('bounded diagnostic listing')
    print('MATRIX_TRACE','\n'.join(code),flush=True);cleanup(f,());print(f'PASS matrix scout; instructions={maximum};heap={p.allocated}',flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
