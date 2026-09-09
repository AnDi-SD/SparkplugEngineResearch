#!/usr/bin/env python3
"""Original texture source field3 writes full MemoryStream buffer and consumes it."""
from pathlib import Path
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager

def main(mode,output):
    if mode not in ('start','positioned'):raise ValueError('Two explicit source cursor cases')
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    f=PCWriteBytesFixture();p=f.p;manager,_=empty_manager(f)
    texture=f.call(0x41a2d0);serializer=f.call(0x42dc30);source=f.call(0x465560)
    assert p.uint(source)==0x6e7e50 and f.allocations[source]==0x38
    # Actual membership walks static records. This supplies just the already
    # proved MemoryStream->Stream->Base registration inputs, not global startup.
    for record,identity,parent in ((0x760280,0x57177db5,0x75ab18),(0x75ab18,0x6cc80d8a,0x755310),(0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    payload=bytes(range(1,18));f.call(0x465500,this=source,args=(len(payload),))
    buffer=p.uint(source+0x30);p.mu.mem_write(buffer,payload)
    cursor=5 if mode=='positioned' else 0
    assert f.call(0x465750,this=source,args=(1,cursor))&255
    p.put_uint(texture+0x0c,source);handled=p.allocate(4);p.put_uint(handled,0xa5a5a5a5)
    result=f.call(0x42e5f0,this=serializer,args=(f.stream,texture,handled))&255
    writer_instructions=sum(p.visits.values())
    expected=b'\xe3'+struct.pack('<I',len(payload))+payload+b'\0'
    assert result==1 and not f.errors and f.data==expected
    assert bytes(p.mu.mem_read(handled,1))==b'\1'
    assert source in f.freed and buffer in f.freed
    dangling=p.uint(texture+0x0c);assert dangling==source
    # Base object does not own/free its generic +0C attachment on destruction.
    f.call(p.uint(p.uint(texture)),this=texture,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    assert set(f.allocations)==set(f.freed)
    report={'status':'passed','mode':mode,'pc_exe_sha256':hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        'source_position_before':cursor,'input_hex':payload.hex(),'output_hex':f.data.hex(),'handled':1,
        'native_source_deleted':True,'native_attachment_left_dangling':True,'native_allocations':len(f.allocations),'native_freed':len(f.freed),
        'writer_instructions':writer_instructions,'arena_bytes':p.allocated,
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),
            'sha256':hashlib.sha256(Path(m.__file__).read_bytes()).hexdigest().upper()} for m in list(sys.modules.values())
            if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Actual MemoryStream factory/allocation/seek, RTTI membership, source writer and teardown. Explicit registration and output-stream fixtures. No attached-source read/global startup claim.'}
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS original embedded source writer',mode,report['native_allocations'],writer_instructions,flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
