#!/usr/bin/env python3
"""Original native texture writer with explicit NPOT single-mip input storage."""
from pathlib import Path
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager

def main(mode,output):
    cases={'1x1':(1,1,1),'13x7':(13,7,1),'1x9-field2':(1,9,2),'3x2-field0':(3,2,0)}
    if mode not in cases:raise ValueError('Explicit small single-mip writer case')
    width,height,field1C=cases[mode];target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    f=PCWriteBytesFixture();p=f.p;manager,_=empty_manager(f)
    for off,value in ((0x10,2),(0x14,2),(0x18,1)):p.put_uint(manager+off,value)
    p.put_uint(0x75dde8,manager);texture=f.call(0x41a2d0);serializer=f.call(0x42b660)
    for entry,value in ((0x435000,1),(0x434fa0,width),(0x434fb0,height),(0x434f80,0),(0x434fe0,field1C)):
        f.call(entry,this=texture,args=(value,))
    pixels=bytes((i*37+11)&255 for i in range(width*height*4));address=p.allocate(len(pixels));f.allocations[address]=len(pixels)
    p.mu.mem_write(address,pixels);vector=p.allocate(16);f.allocations[vector]=16;p.mu.mem_write(vector,struct.pack('<4I',width,height,width*4,address))
    for off,value in ((0x70,vector),(0x74,vector+16),(0x78,vector+16)):p.put_uint(texture+off,value)
    result=f.call(0x42bf40,this=serializer+0x10,args=(f.stream,texture))&255
    writer_instructions=sum(p.visits.values())
    assert result==1 and not f.errors and 0x42b9e0 in p.visits
    encoded=f.data
    f.call(p.uint(p.uint(texture)),this=texture,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    assert set(f.allocations)==set(f.freed)
    report={'status':'passed','mode':mode,'width':width,'height':height,'field1c':field1C,'pixels_hex':pixels.hex(),'output_hex':encoded.hex(),
        'pc_exe_sha256':hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        'native_allocations':len(f.allocations),'native_freed':len(f.freed),'writer_instructions':writer_instructions,'arena_bytes':p.allocated,
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),
            'sha256':hashlib.sha256(Path(m.__file__).read_bytes()).hexdigest().upper()} for m in list(sys.modules.values())
            if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Actual setters and whole DX texture data writer, supplied native mip vector/pixels, complete original teardown. No claim for original vector initialization or texture upload.'}
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS original texture writer',mode,len(f.allocations),writer_instructions,flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
