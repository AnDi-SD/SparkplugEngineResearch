#!/usr/bin/env python3
"""Bounded original-PC DX reader on an exact Importer-written texture slice.

Supports diagnostic rejection of the former bare legacy source and positive
single-mip embedded BGRA acceptance. No replacement of internal game helpers;
only the inherited bounded stream/COM/CRT boundary, no actual GPU or whole game.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from analyze_smo_texture_data import parse_sections
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import empty_manager
from probe_pc_tool_texture_output import ToolOutputFixture,texture_slice,expected_base

def sha(raw):return hashlib.sha256(raw).hexdigest().upper()

def main(path,index,expectation,output):
    if expectation not in ('accepted','rejected'):raise ValueError('explicit acceptance expectation')
    source=Path(path).resolve();target=Path(output).resolve()
    source.relative_to(ROOT);target.relative_to(ROOT/'local-data/results')
    if source.stat().st_size>8*1024*1024:raise ValueError('8MiB source cap')
    raw=source.read_bytes();payload,catalog=texture_slice(raw,int(index))
    fields=parse_sections(payload)
    if len(fields)==1 and fields[0].type==0:
        nested=parse_sections(fields[0].payload)
        if len(nested)!=1 or nested[0].type!=5:raise ValueError('single legacy pixel field')
        width,height,fmt,bpp=struct.unpack_from('<4I',nested[0].payload)
        if fmt!=0 or bpp!=4:raise ValueError('bounded BGRA diagnostic')
        dimensions=(width,height);expected=nested[0].payload[16:];kind='legacy-bare'
    else:
        dimensions,expected=expected_base(payload);kind='embedded-native'
    if len(expected)!=dimensions[0]*dimensions[1]*4 or len(payload)>8192:raise ValueError('bounded exact payload')
    f=ToolOutputFixture(payload,dimensions);p=f.p;manager,_=empty_manager(f)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    obj=f.call(0x4ab520);serializer=f.call(0x42b660);started=time.monotonic()
    report=dict(kind='original-pc-importer-texture',inputPath=str(source.relative_to(ROOT)),inputSha256=sha(raw),
        probeSha256=sha(Path(__file__).read_bytes()),payloadSha256=sha(payload),pixelSha256=sha(expected),
        catalog=catalog,sourceKind=kind,dimensions=dimensions,expectation=expectation,
        limits=dict(profile='file',arenaBytes=p.arena_size,surfaceBytes=f.max_surface_bytes,payloadBytes=8192),
        boundary='Inherited bounded stream/COM/CRT inputs. Actual42C640 and mip helpers; no GPU or whole-file loader.')
    try:
        result=f.call(0x42c640,this=serializer+0x10,args=(f.stream,obj))&255
        recs=getattr(f,'levels',[]);levels=[]
        for rec in recs:
            pixels=b''.join(bytes(p.mu.mem_read(rec['pixels']+y*rec['pitch'],rec['row_bytes'])) for y in range(rec['rows']))
            assert all(bytes(p.mu.mem_read(rec['pixels']+y*rec['pitch']+rec['row_bytes'],4))==b'\xa5'*4 for y in range(rec['rows']))
            levels.append(dict(width=rec['width'],height=rec['height'],pixelSha256=sha(pixels)))
        accepted=(result==1 and f.position==len(payload) and not f.errors and bool(levels)
            and (p.uint(obj+0x28),p.uint(obj+0x2c))==dimensions and levels[0]['pixelSha256']==sha(expected))
        report.update(readerResult=result,cursor=f.position,
            errors=[value.decode('ascii','backslashreplace') if isinstance(value,bytes) else value for value in f.errors],accepted=accepted,
            nativeState=[p.uint(obj+off) for off in (0x18,0x20,0x28,0x2c)],levels=levels,events=f.events.copy(),
            nativeInstructions=sum(p.visits.values()))
        assert accepted==(expectation=='accepted'),'original reader outcome differs from explicit expectation'
        if expectation=='rejected':assert not levels,'diagnostic rejection must not create a partial pixel surface'
        f.call(0x4abb50,this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
        for address in (0x75db78,0x75526c,0x755264):
            owned=p.uint(address)
            if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
        assert set(f.allocations)==set(f.freed),'balanced actual allocations'
        assert f.texture_refs==f.surface_refs==0 and f.device_refs==1,'balanced COM lifetime'
        assert all(rec['refs']==0 and not rec['locked'] for rec in recs)
        report.update(status='passed',releasedAllocations=len(f.freed))
    except (AssertionError,ValueError) as error:
        report.update(status='failed',error=str(error),stoppedIp=f'{p.reg("EIP"):08X}')
        raise
    finally:
        report.update(seconds=time.monotonic()-started,arenaReservedBytes=p.allocated)
        target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({k:v for k,v in report.items() if k not in ('levels','events')}),flush=True)
    return 0

if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
