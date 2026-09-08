#!/usr/bin/env python3
"""Original-PC load/teardown of a small Importer-produced static scene.

Reuses declared RTTI, stream, CRT and COM inputs from the sealed scene harness.
This native-only diagnostic deliberately does not require portable-source
acceptance first, and does not claim a portable/native comparison or game boot.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import empty_manager,empty_fat
from probe_pc_scene_file_profile import CrystalSceneFileFixture,seed_scene_rtti,capture_scene
from probe_pc_texture_missing_mips import MissingMipFixture
from probe_pc_tool_texture_output import texture_slice,expected_base

def sha(raw):return hashlib.sha256(raw).hexdigest().upper()
def main(input_path,report_path):
    source=Path(input_path).resolve();source.relative_to(ROOT)
    output=Path(report_path).resolve();output.relative_to(ROOT/'local-data/results')
    if source.stat().st_size>32768:raise ValueError('32KiB static scene cap')
    raw=source.read_bytes();count=struct.unpack_from('<I',raw,28)[0]
    if not 1<=count<=32:raise ValueError('1..32 objects')
    offset=32;shapes=[]
    for index in range(count):
        size=struct.unpack_from('<H',raw,offset+4)[0];kind=struct.unpack_from('<I',raw,offset+6+size)[0]
        if kind==0x78ea082b:
            payload,_=texture_slice(raw,index);shape,_=expected_base(payload);shapes.append(shape)
        offset+=18+size
    if not 1<=len(shapes)<=2 or not all(1<=n<=32 for shape in shapes for n in shape):raise ValueError('declared small textures')
    f=CrystalSceneFileFixture(raw);MissingMipFixture.install_many_on_scene(f,tuple(shapes),surface_profile='tool32');p=f.p
    f.call(0x6d38e0)
    seed_scene_rtti(f,with_material=True,with_texture=True)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
    for kind,factory,platform in [(0x695c0f65,0x4638f0,255),(0x603625d0,0x469040,255),
        (0x763277db,0x4934c0,255),(0x6160348b,0x42f690,255),(0x7ac95aec,0x43b830,255),
        (0x33c34cf0,0x4297c0,2),(0x78ea082b,0x42b660,6)]:
        serializer=f.call(factory);f.call(0x422d90,this=manager,args=(kind,serializer,platform,1))
    f.call(0x45adf0);published=[]
    def observe(mu,address,size,user):
        if p.uint(fat+0x50):
            head=p.uint(fat+0x4c);at=p.uint(head);rows=[]
            while at!=head:
                assert len(rows)<32;entry=p.uint(at+8)
                rows.append([p.uint(entry+4),p.uint(entry+0x10),p.uint(entry+0x14),p.uint(entry+0x20)]);at=p.uint(at)
            published.append(rows)
    observer=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=0x466760,end=0x466760)
    report=dict(kind='original-pc-importer-static-scene',inputPath=str(source.relative_to(ROOT)),inputSha256=sha(raw),
        probeSha256=sha(Path(__file__).read_bytes()),objects=count,dimensions=shapes,
        limits=dict(profile='file',arenaBytes=p.arena_size,objects=32,inputBytes=32768,processSeconds=30),
        boundary='Prepared RTTI/startup, bounded stream/CRT/COM inputs. Whole original loader and destructors; no game startup/GPU/portable comparison.')
    start=time.monotonic()
    try:
        root=f.call(0x422b50,this=manager,args=(f.stream,));p.mu.hook_del(observer)
        report.update(root=f'{root:08X}',cursor=f.position,wholeLoadInstructions=sum(p.visits.values()),publishedCounts=[len(v) for v in published])
        assert root and not f.errors and f.position==len(raw),'complete original load without diagnostics'
        assert len(published)==1 and len(published[0])==count,'all catalogued objects published'
        report['scene']=capture_scene(f,root,published[0],True)
        combiners=[a for a,s in f.allocations.items() if a not in f.freed and s==0x2c and p.uint(a)==0x6ef294]
        assert len(combiners)==1
        f.call(p.uint(p.uint(root)),this=root,args=(1,))
        for obj in combiners:f.call(0x4a9f30,this=obj,args=(1,))
        f.clear_declarations();f.call(0x4228a0,this=manager)
        for address in (0x75db90,0x75db78,0x75526c,0x755264):
            obj=p.uint(address)
            if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
        assert all(b['refs']==0 and b['locks']==0 for b in f.buffers.values())
        for io in f.texture_ios:
            assert io.device_refs==1 and io.texture_refs==io.surface_refs==0 and not io.locked
            assert all(rec['refs']==0 and not rec['locked'] for rec in io.levels)
        assert set(f.allocations)==set(f.freed)
        report.update(status='passed',releasedAllocations=len(f.freed))
    except (AssertionError,ValueError) as error:
        report.update(status='failed',error=str(error),stoppedIp=f'{p.reg("EIP"):08X}',cursor=f.position)
        raise
    finally:
        report.update(seconds=time.monotonic()-start,arenaReservedBytes=p.allocated,
            diagnostics=[v.decode('ascii','backslashreplace') if isinstance(v,bytes) else v for v in f.errors],ioTail=f.io[-25:])
        output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({k:v for k,v in report.items() if k not in ('scene','ioTail')}),flush=True)
    return 0

if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
