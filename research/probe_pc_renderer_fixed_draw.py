#!/usr/bin/env python3
"""Whole original4BC290 automatic no-blend-weight branch; external COM only."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from probe_pc_renderer_draw import DrawFixture
from probe_pc_function_eval import cleanup

def main(mode,return_capture=False):
    if mode not in ('fixed','fixed-failed','fixed-pixel'):raise ValueError('bounded whole no-weight automatic branch')
    f=DrawFixture(mode=='fixed-failed');p=f.p;r=f.renderer;material=f.call(0x4a9460);p.put_uint(material+0xb8,0);p.put_uint(r+0xe47c,material)
    checks=0;maximum=0;captures=[];shader=p.allocate(0x54);p.put_uint(shader+0x50,101);f.tokens[shader]=1
    p.put_uint(r+0xe44c,shader)
    p.mu.mem_write(r+0xf2f5,bytes([int(mode=='fixed-pixel')]))
    if mode=='fixed-pixel':p.put_uint(r+0xe464,shader)
    for step,kind in enumerate((0,1,2,3,4,2)):
        args=(kind,11,13,17,19,0x801,23);f.events.clear();result=f.call(0x4bc290,this=r,args=args)&255
        maximum=max(maximum,sum(p.visits.values()));checks+=1
        if result!=1 or not p.visits.get(0x4be310) or not p.visits.get(0x4c8980) or not p.visits.get(0x4be2b0) or p.visits.get(0x4c89eb) or p.visits.get(0x4ae930):raise AssertionError('actual automatic no-weight chain; no fake compiled shader/constants')
        captures.append([step,list(args),result,f.state(),list(f.events)])
    manager=p.uint(0x763024);cleanup(f,(material,manager));checks+=1;capture=[mode,captures]
    if not return_capture:print('RENDERER_FIXED_DRAW_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: actual no-weight automatic draw {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
