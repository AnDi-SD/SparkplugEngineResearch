"""Original Font reader owns the actual runtime DXTexture, not TextureData.
The reference resolver is an explicit fixture leaf returning the actual factory
object. TextureDeviceFixture supplies only borrowed renderer/COM refcount leaves.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import run_bounded
from pc_texture_fixtures import TextureDeviceFixture
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def guest(folder):
    out=ROOT/folder;out.mkdir(parents=True,exist_ok=False)
    (out/'probe-source.py').write_bytes(Path(__file__).read_bytes())
    f=TextureDeviceFixture();p=f.p;start=time.perf_counter()
    report=dict(kind='original-pc-font-runtime-atlas-ownership',status='running',pc=sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
      profile='micro',arena=65536,allocation_bound=32768,child_timeout_seconds=30,calls=[],
      new_seam='4678B0: declared existing reference ID2345 -> actual DXTexture4AB520, no reference increment')
    def call(entry,this=0,args=()):
        row=dict(entry=f'{entry:08X}');report['calls'].append(row)
        try:value=f.call(entry,this=this,args=args);row['returned']=True;return value
        except Exception as error:row.update(error=repr(error),eip=f'{p.reg("EIP"):08X}');raise
        finally:row.update(instructions=sum(p.visits.values()),limits=p.last_execution_limits)
    try:
        texture=call(0x4ab520);font=call(0x462ec0);serializer=call(0x442500)
        def refs():return int.from_bytes(p.mu.mem_read(texture+8,2),'little')
        report['before']=dict(texture=f'{texture:08X}',vtable=f'{p.uint(texture):08X}',size=f.allocations[texture],refs=refs())
        def resolve(p):
            assert p.uint(p.reg('ESP')+4)==0x2f281e13 and p.uint(p.reg('ESP')+8)==f.stream
            assert f.data[f.position:f.position+8]==struct.pack('<II',0x2345,0)
            f.position+=8;p.fixture_return(12,eax=texture)
        p.seams[0x4678b0]=resolve
        data=struct.pack('<4I',0x2345,0,12,3)+bytes(224*17)
        wire=b'\xe0'+struct.pack('<I',len(data))+data+b'\0';f.data=wire
        (out/'font-input.bin').write_bytes(wire)
        assert call(0x442660,this=serializer+0x10,args=(f.stream,font))&255 and f.position==len(wire)
        report['after']=dict(atlas=f'{p.uint(font+0x1c):08X}',refs=refs())
        assert p.uint(font+0x1c)==texture and refs()==1
        call(p.uint(p.uint(font)),this=font,args=(1,));call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
        report['released']=all(v in f.freed for v in (texture,font,serializer))
        assert report['released'] and f.device_refs==1 and f.device_events==['AddRef','Release'] and not f.errors
        report['status']='passed-scoped-ownership'
    except Exception as error:report.update(status='blocked',error=repr(error))
    report.update(elapsed_seconds=time.perf_counter()-start,arena_used=p.allocated,device_events=f.device_events,
      allocations=[dict(address=f'{a:08X}',size=n,freed=a in f.freed) for a,n in f.allocations.items()])
    paths={Path(__file__).resolve()};paths.update(Path(m.__file__).resolve() for m in list(sys.modules.values())
      if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research'))
    report['dependencies']=[dict(path=x.relative_to(ROOT).as_posix(),sha256=sha(x)) for x in sorted(paths)]
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report.get(k) for k in ('status','error','before','after','released','elapsed_seconds','arena_used')}))
    return 0 if report['status'].startswith('passed') else 1
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(guest(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
