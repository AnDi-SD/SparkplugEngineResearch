#!/usr/bin/env python3
"""Bounded original read-time state for the tool UV/color field adapters."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup,configure_floor,bits
from probe_pc_node_serializer import field
from probe_pc_material_color import controller_input,OFFSETS as COLOR_OFFSETS
from probe_pc_uv_functions import OFFSETS as UV_OFFSETS

MODES=('uv-default','uv-values','uv-repeat-wide','uv-ieee','color-default','color-values','color-repeat-ieee')
def scalar(p,base):
    return struct.pack('<I',p.uint(base+0x34))+b''.join(bytes(p.mu.mem_read(base+o,4)) for o in (0x14,0x1c,0x20,0x24,0x28))

def main(mode):
    if mode not in MODES:raise ValueError('Explicit bounded reader case required')
    f=PCWriteBytesFixture();p=f.p;configure_floor(f);manager=f.call(0x454640)
    is_uv=mode.startswith('uv-');controller=f.call(0x41a210) if is_uv else controller_input(f)
    serializer=f.call(0x440b00 if is_uv else 0x441200)
    sections=[]
    for i in range(7 if is_uv else 5):
        color=not is_uv and i<4;offset=2 if color else 0;section=b''
        if 'values' in mode:
            if color:section+=field(0,struct.pack('<I',0x80402010+i))+field(1,struct.pack('<I',0xff112244+i))
            section+=field(offset,struct.pack('<I',8))+field(offset+1,struct.pack('<f',.25*(i+1)))
            section+=field(offset+2,struct.pack('<f',1.5))+field(offset+3,struct.pack('<f',-.25))
            section+=field(offset+4,struct.pack('<f',i*.5))+field(offset+5,struct.pack('<f',.125))
        if 'repeat' in mode and i==0:
            section=field(15,b'ignore')+field(0,struct.pack('<I',1))+field(0,struct.pack('<I',0x80402010 if color else 8))
            section+=field(offset+1,struct.pack('<f',-.5))
        if 'ieee' in mode and i==0:
            section+=field(offset+2,struct.pack('<I',0x7fc12345))+field(offset+3,struct.pack('<I',0x80000000))
        sections.append(section+b'\0')
    body=b''.join(sections)
    if is_uv:
        vectors=struct.pack('<6f',.5,1,2,0,0,1)
        # Width is reader input; the original writer keeps its own UInt8 choice.
        transform=(b'\xe0'+struct.pack('<I',len(body)+24)+body+vectors if 'wide' in mode or len(body)+24>255 else field(0,body+vectors))+b'\0'
        body=transform
    wire=b'\xe0'+struct.pack('<I',len(body))+body+b'\0';f.data=wire;f.position=0
    result=f.call(0x440be0 if is_uv else 0x4412e0,this=serializer+0x10,args=(f.stream,controller))&255
    assert result==1 and f.position==len(wire) and not f.errors,(result,f.position,f.errors)
    if is_uv:
        trans=controller+0x4c
        observed=b''.join(scalar(p,trans+o) for o in UV_OFFSETS)+bytes(p.mu.mem_read(trans+0x160,24))
    else:
        observed=b''.join(bytes(p.mu.mem_read(controller+o+0x10,8))+scalar(p,controller+o+0x18) for o in COLOR_OFFSETS)+scalar(p,controller+0x1a8)
    f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    if is_uv:f.call(p.uint(p.uint(controller)),this=controller,args=(1,))
    f.call(0x4545d0,this=manager,args=(1,));cleanup(f,())
    assert set(f.allocations)==set(f.freed)
    print('TOOL_FUNCTION_CAPTURE '+json.dumps({'mode':mode,'wire':wire.hex(),'body':body.hex(),'state':observed.hex(),
        'scope':'Original whole UV reader on actual controller' if is_uv else 'Original whole color reader on declared backing with actual leaf defaults; capped constructor excluded'}),flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
