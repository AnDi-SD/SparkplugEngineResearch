#!/usr/bin/env python3
"""Original4AD540 refresh and all three lazy getters, no external calls."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_function_eval import cleanup,bits

def main(mode,return_capture=False):
    if mode not in ('direct','view','vp','wvp','constants'):raise ValueError('bounded complete matrix consumer')
    f=ApplyFixture(renderer_size=0xf2f8);p=f.p;r=f.renderer;checks=0;maximum=0;captures=[];owned=[]
    shader=output=0
    if mode=='constants':
        shader=f.call(0x4c9f10);owned.append(shader);p.run(0x412400,args=(44,),callee_pop=False);desc=p.reg('EAX')
        p.mu.mem_write(desc,bytes(44));p.put_uint(desc+0x20,1);p.put_uint(desc+0x28,4)
        p.put_uint(shader+0x3c,desc);p.put_uint(shader+0x40,desc+44);p.put_uint(shader+0x44,desc+44);output=p.allocate(64)
    seed=0x12345678
    for case in range(12):
        for matrix,off in enumerate((0xca40,0xca80,0xcac0)):
            for i in range(16):
                seed=(seed*1664525+1013904223)&0xffffffff
                value=((seed>>24)-128)/32. if case else float((i+matrix)%5-2)
                p.put_uint(r+off+4*i,bits(value))
        p.mu.mem_write(r+0xf2f4,b'\1')
        for repeat in range(2):
            if repeat:p.put_uint(r+0xca40,bits(99.)) # dirty remains0; direct always recomputes, getters do not
            entry={'direct':0x4ad540,'view':0x4ad640,'vp':0x4ad680,'wvp':0x4ad660,'constants':0x4ae930}[mode]
            result=f.call(entry,this=shader if shader else r if mode=='direct' else r+0x18,args=(output,) if shader else ())
            maximum=max(maximum,sum(p.visits.values()));checks+=1
            if f.events or p.mu.mem_read(r+0xf2f4,1)!=b'\0':raise AssertionError('native pure cache refresh clears dirty')
            if mode not in ('direct','constants') and result!=r+{'view':0xcb00,'vp':0xcb40,'wvp':0xcb80}[mode]:raise AssertionError('actual lazy getter pointer')
            captures.append([case,repeat,bool(p.visits.get(0x4ad540)),[[p.uint(r+off+4*i) for i in range(16)] for off in (0xcb00,0xcb40,0xcb80)],[] if not shader else [p.uint(output+4*i) for i in range(16)]])
    cleanup(f,owned);checks+=1;capture=[mode,captures]
    if not return_capture:print('RENDERER_MATRICES_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original renderer matrices {mode}; maxInstructions={maximum};heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
