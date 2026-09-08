#!/usr/bin/env python3
"""Original-PC load/teardown of a small Importer-produced skinned scene.

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

class CharacterSceneFixture(CrystalSceneFileFixture):
    guest_execution_profile='character'
    guest_arena_size=262144

def seed_preserved_collision_rtti(f):
    # Exact identities, bases, record addresses and factories from the pristine
    # registration initializers 6D39A0 / 6D4610. These preserved resources need
    # registration in this prepared environment, not reconstructed behavior.
    p=f.p
    for rec,identity,parent in [(0x760220,0x47a97c0e,0x755310),
        (0x7617d0,0x4da04889,0x7629e0),(0x7629e0,0x21cc76af,0x755310)]:
        p.put_uint(rec,identity);p.put_uint(rec+0x48,parent)
    tree=p.uint(0x755378);old_head=p.uint(tree+0x18);rows=[]
    def collect(node):
        if node==old_head:return
        assert len(rows)<32
        collect(p.uint(node));rows.append((p.uint(node+12),p.uint(node+16)));collect(p.uint(node+8))
    collect(p.uint(old_head+4))
    for identity,record,factory in [(0x47a97c0e,0x760220,0x4653a0),(0x4da04889,0x7617d0,0x4879c0)]:
        rows.append((identity,record));p.put_uint(record+0x4c,factory)
    rows.sort();assert len({r[0] for r in rows})==len(rows)
    head=p.allocate(24);red_level=(len(rows)+1).bit_length()-1
    def build(part,parent,depth=0):
        if not part:return head
        mid=len(part)//2;identity,record=part[mid];node=p.allocate(24)
        p.put_uint(node+4,parent);p.put_uint(node+12,identity);p.put_uint(node+16,record)
        p.mu.mem_write(node+20,bytes((int(depth!=red_level),0)))
        p.put_uint(node,build(part[:mid],node,depth+1));p.put_uint(node+8,build(part[mid+1:],node,depth+1));return node
    root=build(rows,head);left=right=root
    while p.uint(left)!=head:left=p.uint(left)
    while p.uint(right+8)!=head:right=p.uint(right+8)
    p.put_uint(head,left);p.put_uint(head+4,root);p.put_uint(head+8,right);p.mu.mem_write(head+20,b'\x01\x01')
    p.put_uint(tree+0x18,head);p.put_uint(tree+0x1c,len(rows))
    def validate(node,parent,lo=-1,hi=0x100000000):
        if node==head:return 1
        identity=p.uint(node+12);assert lo<identity<hi and p.uint(node+4)==parent
        a,b=p.uint(node),p.uint(node+8);black=bool(p.mu.mem_read(node+20,1)[0])
        if not black:assert all(c==head or p.mu.mem_read(c+20,1)[0] for c in (a,b))
        x,y=validate(a,node,lo,identity),validate(b,node,identity,hi);assert x==y;return x+black
    assert p.mu.mem_read(root+20,1)[0];validate(root,head)
    for identity,_ in rows:assert f.call(0x4143f0,this=tree,args=(identity,))&255

def main(input_path,report_path):
    source=Path(input_path).resolve();source.relative_to(ROOT)
    output=Path(report_path).resolve();output.relative_to(ROOT/'local-data/results')
    if source.stat().st_size>32768:raise ValueError('32KiB skinned scene cap')
    raw=source.read_bytes();count=struct.unpack_from('<I',raw,28)[0]
    if not 1<=count<=256:raise ValueError('1..256 objects')
    offset=32;shapes=[]
    for index in range(count):
        size=struct.unpack_from('<H',raw,offset+4)[0];kind=struct.unpack_from('<I',raw,offset+6+size)[0]
        if kind==0x78ea082b:
            payload,_=texture_slice(raw,index);shape,_=expected_base(payload);shapes.append(shape)
        offset+=18+size
    if not 1<=len(shapes)<=2 or not all(1<=n<=32 for shape in shapes for n in shape):raise ValueError('declared small textures')
    f=CharacterSceneFixture(raw);MissingMipFixture.install_many_on_scene(f,tuple(shapes),surface_profile='tool32');p=f.p
    f.call(0x6d38e0)
    seed_scene_rtti(f,with_light=True,with_material=True,with_texture=True,with_skin=True)
    seed_preserved_collision_rtti(f)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
    for kind,factory,platform in [(0x695c0f65,0x4638f0,255),(0x603625d0,0x469040,255),
        (0x763277db,0x4934c0,255),(0x6160348b,0x42f690,255),(0x7ac95aec,0x43b830,255),
        (0x33c34cf0,0x4297c0,2),(0x78ea082b,0x42b660,6),(0x681f2043,0x490c50,255),
        (0x47a97c0e,0x438960,255),(0x4da04889,0x439a50,255),(0x5e6402df,0x43ffd0,255)]:
        serializer=f.call(factory);f.call(0x422d90,this=manager,args=(kind,serializer,platform,1))
    f.call(0x45adf0);published=[]
    def observe(mu,address,size,user):
        if p.uint(fat+0x50):
            head=p.uint(fat+0x4c);at=p.uint(head);rows=[]
            while at!=head:
                assert len(rows)<256;entry=p.uint(at+8)
                rows.append([p.uint(entry+4),p.uint(entry+0x10),p.uint(entry+0x14),p.uint(entry+0x20)]);at=p.uint(at)
            published.append(rows)
    observer=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=0x466760,end=0x466760)
    report=dict(kind='original-pc-importer-skinned-scene',inputPath=str(source.relative_to(ROOT)),inputSha256=sha(raw),
        probeSha256=sha(Path(__file__).read_bytes()),objects=count,dimensions=shapes,
        limits=dict(profile='character',arenaBytes=p.arena_size,objects=256,inputBytes=32768,processSeconds=30,instructionsPerCall=4000000,microsecondsPerCall=16000000),
        boundary='Prepared RTTI/startup, bounded stream/CRT/COM inputs. Whole original loader and destructors; no game startup/GPU/portable comparison.')
    start=time.monotonic()
    try:
        root=f.call(0x422b50,this=manager,args=(f.stream,));p.mu.hook_del(observer)
        report.update(root=f'{root:08X}',cursor=f.position,wholeLoadInstructions=sum(p.visits.values()),publishedCounts=[len(v) for v in published])
        assert root and not f.errors and f.position==len(raw),'complete original load without diagnostics'
        assert len(published)==1 and len(published[0])==count,'all catalogued objects published'
        known=[];opaque=[]
        for row in published[0]:
            obj=row[3];record=f.call(p.uint(p.uint(obj)+0x10),this=obj);kind=p.uint(record)
            if kind in (0x47a97c0e,0x4da04889):
                assert obj in f.allocations
                opaque.append(dict(fileId=row[0],runtimeClass=kind,nativeAllocationBytes=f.allocations[obj]))
            else:known.append(row)
        report['preservedCollisionObjects']=opaque
        report['scene']=capture_scene(f,root,known,True)
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
        # The preserved collision records lazily create the original global
        # spCollisionManager. Whole application shutdown is outside this probe;
        # explicitly invoke its original deleting destructor after scene teardown.
        # Exact registration 6D3610: getter record 75FB18, vtable 6E6FE4, size1790.
        collision_managers=[a for a,s in f.allocations.items() if a not in f.freed and s==0x1790 and p.uint(a)==0x6e6fe4]
        assert len(collision_managers)==int(bool(opaque)),'one original collision singleton for preserved collision objects'
        report['collisionManagerCleanup']=[]
        for obj in collision_managers:
            assert f.call(p.uint(p.uint(obj)+0x10),this=obj)==0x75fb18
            slots=[a for a in range(0x75db50,0x75dbb0,4) if p.uint(a)==obj]
            assert len(slots)==1,'collision manager has one engine singleton owner slot'
            f.call(0x4571c0,this=obj,args=(1,))
            report['collisionManagerCleanup'].append(dict(vtable='006E6FE4',ownerSlot=f'{slots[0]:08X}',released=obj in f.freed))
        assert set(f.allocations)==set(f.freed),'all native allocations released'
        report.update(status='passed',releasedAllocations=len(f.freed))
    except (AssertionError,ValueError) as error:
        report.update(status='failed',error=str(error),stoppedIp=f'{p.reg("EIP"):08X}',cursor=f.position)
        raise
    finally:
        report['remainingAllocations']=[dict(address=f'{a:08X}',size=s,head=bytes(p.mu.mem_read(a,min(s,16))).hex())
            for a,s in f.allocations.items() if a not in f.freed]
        report.update(seconds=time.monotonic()-start,arenaReservedBytes=p.allocated,
            diagnostics=[v.decode('ascii','backslashreplace') if isinstance(v,bytes) else v for v in f.errors],ioTail=f.io[-25:])
        output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({k:v for k,v in report.items() if k not in ('scene','ioTail','remainingAllocations','preservedCollisionObjects')}),flush=True)
    return 0

if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
