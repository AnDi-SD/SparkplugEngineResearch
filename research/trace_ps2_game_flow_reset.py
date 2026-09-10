#!/usr/bin/env python3
"""Strict straight-line low-word provenance for 33B590..33B8D4, not execution.

Tracks only the audited LUI/ADDIU/LW/SW and stack SQ/LQ/SD subset. Stops at
the first JAL including its delay slot; the callee and post-call path are not
interpreted. Unknown instructions or non-stack spill aliases abort.
"""
from pathlib import Path
import json,struct,sys,hashlib
from capture_native_ranges import ROOT,PS2,EXPECTED,read_window,read_elf_sections


def trace(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    raw=PS2.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2']
    sections=read_elf_sections(raw)
    catalog=json.loads((ROOT/'local-data/results/native-cycle-20260910-1900/catalog/ps2-architecture.json').read_text())
    gp=catalog['global_pointer'];entry=0x33b590
    body,fileoff=read_window('ps2',raw,entry,0x348,sections)
    # Only low 32 bits are needed by SW and address arithmetic; upper128 bits
    # are neither claimed nor synthesized. Aligned stack spills preserve this
    # tracked low word exactly; no overlapping stack access occurs here.
    regs=[{'unknownRegister':i} for i in range(32)];regs[0]=0;regs[28]=gp;regs[29]=0x20001000
    stack={};writes=[];call=None
    for i in range(0,len(body),4):
        pc=entry+i;w=struct.unpack_from('<I',body,i)[0]
        op=w>>26;rs=(w>>21)&31;rt=(w>>16)&31;imm=struct.unpack('<h',body[i:i+2])[0]
        if op==0x0f:regs[rt]=(w&0xffff)<<16
        elif op==9:
            assert isinstance(regs[rs],int),(hex(pc),'nonconstant ADDIU')
            regs[rt]=(regs[rs]+imm)&0xffffffff
        elif op in (0x23,0x2b,0x1e,0x1f,0x3f):
            assert isinstance(regs[rs],int),(hex(pc),'nonconstant base')
            address=(regs[rs]+imm)&0xffffffff
            if op in (0x1e,0x1f,0x3f):
                assert rs==29 and address%16==0 and 0x20000e90<=address<0x20001000
                if op==0x1e:regs[rt]=stack[address]
                else:stack[address]=regs[rt]
            elif op==0x23:
                assert rs==28
                value=struct.unpack('<I',read_window('ps2',raw,address,4,sections)[0])[0]
                regs[rt]=dict(globalAddress=address,loadPc=pc,imageValue=value)
            else:
                assert 0x4c40f0<=address<0x4c41d0 and address%4==0
                assert isinstance(regs[rt],dict) and 'globalAddress' in regs[rt],(hex(pc),regs[rt])
                writes.append(dict(destination=address,storePc=pc,**regs[rt]))
        elif w==0x0000282d:regs[5]=0 # DADDU a1,zero,zero: memset fill byte
        elif op==3:
            assert call is None and pc==0x33b8d0
            call=dict(address=pc,target=((pc+4)&0xf0000000)|((w&0x3ffffff)<<2))
        else:raise ValueError(f'Unreviewed instruction {w:08X} at {pc:08X}')
        regs[0]=0
    assert len(writes)==56 and sorted(x['destination'] for x in writes)==list(range(0x4c40f0,0x4c41d0,4))
    assert call['target']==0x4076a8
    call['arguments']=[regs[i] for i in (4,5,6)]
    assert call['arguments']==[0x4c41e0,0,0xe0]
    result=dict(kind='static-ps2-game-flow-reset-provenance',status='passed',execution=False,
        executableSha256=EXPECTED['ps2'],sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        entry=entry,byteSize=len(body),fileOffset=fileoff,bytes=body.hex(),globalPointer=gp,
        writes=sorted(writes,key=lambda x:x['destination']),calleeBoundary=call,
        scope='Straight-line original global-load to store provenance only, with tracked low words of disjoint stack spills. No PS2 execution or callee simulation.')
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status='passed',globalMoves=len(writes),callee=call)))


if __name__=='__main__':trace(sys.argv[1])
