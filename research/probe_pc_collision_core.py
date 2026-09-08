#!/usr/bin/env python3
"""Bounded original-PC collision metadata/ownership evidence for tools cores.

Real constructors, field bodies, clone, Node attach/update/detach and destructors.
Only the leaf reference lookup/write is an explicit typed fixture in field-only
cases; whole-file reference resolution is checked by the separate scene probe.
No collision query, scene broadphase, GPU or game startup claim.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_serializer import field

IDENTITY=(1.,0.,0.,0.,1.,0.,0.,0.,1.)
P=(7.,8.,9.)
Q=(0.,0.,.5,.5) # deliberately non-unit; original does not normalize
S=(2.,3.,4.)

def state(f,obj):
    p=f.p
    return dict(group=p.uint(obj+0x18),owner=p.uint(obj+0x10),primitive=p.uint(obj+0x14),
        position=p.floats(obj+0x20,3),orientation=p.floats(obj+0x2c,9),
        scale=p.floats(obj+0x50,3),sphere=p.floats(obj+0x5c,4))

def cleanup(f,objects):
    p=f.p
    for obj in objects:
        if obj not in f.freed:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    for slot in (0x75db90,0x75db6c,0x75db78,0x75526c,0x755264,0x74e060):
        obj=p.uint(slot)
        if obj and obj not in f.freed:
            f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    assert set(f.allocations)==set(f.freed),'every native allocation released'

def main(mode,output,source=None):
    start=time.monotonic()
    if mode=='graph':return graph_case(source,output)
    if mode=='obb':return obb_case(output)
    if mode not in ('defaults','transform','primitive','replace','clone','node','node-rotation'):
        raise ValueError('explicit collision micro case required')
    data=b'\0'
    if mode in ('transform','primitive','replace'):
        data=field(1,struct.pack('<I',0x12345678))+field(2,struct.pack('<10f',*P,*Q,*S))
        if mode in ('primitive','replace'):data=field(0,struct.pack('<I',11))+data
        if mode=='replace':data+=field(0,struct.pack('<I',12))+field(0,struct.pack('<I',12))
        data+=b'\0'
    f=PCWriteBytesFixture(data);p=f.p
    f.call(0x6d38e0)
    info=f.call(0x4653a0);serializer=f.call(0x438960)
    assert f.allocations[info]==0x8c and p.uint(info)==0x6e7e24
    assert state(f,info)==dict(group=1,owner=0,primitive=0,position=(0.,)*3,
        orientation=IDENTITY,scale=(1.,)*3,sphere=(0.,)*4)
    refs={};lookups=[];writes=[];objects=[info,serializer]
    if mode in ('primitive','replace','clone','node','node-rotation'):
        refs[11]=f.call(0x4879c0);objects.append(refs[11])
        assert f.allocations[refs[11]]==0x178
        assert p.floats(refs[11]+0x28,9)==IDENTITY and p.floats(refs[11]+0x58,3)==(1.,)*3
    if mode=='replace':refs[12]=f.call(0x4879c0);objects.append(refs[12])
    def resolve(p):
        sp=p.reg('ESP');kind,stream,origin=[p.uint(sp+i) for i in (4,8,12)]
        assert kind==0x21cc76af and stream==origin==f.stream
        key=struct.unpack_from('<I',f.data,f.position)[0];f.position+=4
        lookups.append(key);p.fixture_return(12,eax=refs[key])
    def write_ref(p):
        sp=p.reg('ESP');stream,obj=[p.uint(sp+i) for i in (4,8)]
        assert stream==f.stream and obj in refs.values()
        key=next(k for k,v in refs.items() if v==obj);writes.append(key)
        raw=struct.pack('<I',key);end=f.position+4
        f.data=f.data[:f.position]+raw+f.data[end:];f.position=end
        p.fixture_return(8,eax=1)
    p.seams[0x4678b0]=resolve;p.seams[0x467350]=write_ref
    result=f.call(0x438a80,this=serializer+0x10,args=(f.stream,info))&255
    assert result==1 and f.position==len(data) and not f.errors
    captured=dict(mode=mode,input=data.hex(),read=state(f,info),lookups=lookups)
    if mode in ('transform','primitive','replace'):
        assert p.uint(info+0x18)==0x12345678
        assert p.floats(info+0x20,3)==P and p.floats(info+0x50,3)==S
        assert p.floats(info+0x2c,9)==(.5,.5,0.,-.5,.5,0.,0.,0.,1.)
    if mode=='replace':
        assert refs[11] in f.freed and p.uint(refs[12]+8)&65535==1
        assert lookups==[11,12,12]
    if mode=='clone':
        # Original clone-map static constructor and root transaction. Direct
        # invocation without this startup table is not a valid clone fixture.
        f.call(0x52fd90,this=0x755588)
        manager=f.call(0x412540);p.put_uint(0x74e060,manager)
        p.put_uint(info+0x14,refs[11]);p.put_uint(refs[11]+8,1)
        p.put_uint(info+0x18,9);p.put_floats(info+0x20,P);p.put_floats(info+0x50,S)
        copied=f.call(0x412be0,this=manager,args=(info,));objects.insert(0,copied)
        captured['clone']=state(f,copied)
        assert p.uint(copied+0x14)==refs[11] and p.uint(refs[11]+8)&65535==2
        assert p.uint(copied+0x18)==9 and p.floats(copied+0x20,3)==(0.,)*3
        assert p.floats(copied+0x50,3)==(1.,)*3,'clone does not copy transform'
    if mode in ('node','node-rotation'):
        node=f.call(0x421e20);objects.insert(0,node)
        for rec,kind,parent in ((0x755310,0x415352a1,0),(0x7555f8,0x44de07fd,0x755310),
            (0x75dd88,0x695c0f65,0x7555f8)):
            p.put_uint(rec,kind);p.put_uint(rec+0x48,parent)
        p.put_uint(info+0x14,refs[11]);p.put_uint(refs[11]+8,1)
        # A confirmed read-like primitive source/mirror state, not query data.
        p.put_floats(refs[11]+0x18,(1.,2.,3.,4.));p.put_floats(refs[11]+0x4c,(1.,2.,3.))
        p.put_floats(info+0x20,(100.,200.,300.))
        if mode=='node-rotation':
            p.put_floats(node+0x40,(0.,1.,0.,-1.,0.,0.,0.,0.,1.))
            p.put_floats(refs[11]+0x28,(.5,.5,0.,-.5,.5,0.,0.,0.,1.))
        f.call(0x421ed0,this=node,args=(info,))
        assert p.uint(info+0x10)==node and p.uint(node+0x6c)-p.uint(node+0x68)==4
        assert p.uint(info+8)&65535==0,'Node owns collision directly, no intrusive increment'
        p.put_floats(node+0x20,P);p.put_floats(node+0x30,S);p.put_uint(node+0xb0,p.uint(node+0xb0)|1)
        f.call(0x421420,this=node,args=(0,))
        captured['nodeUpdate']=state(f,info)
        captured['obbAfterUpdate']=list(p.floats(refs[11]+0x18,26))
        center=(8.,10.,12.) if mode=='node' else (5.,9.,12.)
        assert p.floats(info+0x20,3)==center and p.floats(info+0x50,3)==S
        assert p.floats(info+0x5c,4)==(*center,4.),'sphere offset/radius not scaled here'
        if mode=='node-rotation':assert p.floats(info+0x2c,9)==(-.5,.5,0.,-.5,-.5,0.,0.,0.,1.)
        assert 0x486930 in p.visits,'real OBB update virtual ran'
        f.call(0x421690,this=node,args=(info,0))
        assert p.uint(info+0x10)==0 and p.uint(node+0x68)==p.uint(node+0x6c)
        captured['detached']=state(f,info)
        f.call(0x4651e0,this=info)
        captured['unownedUpdate']=state(f,info)
        assert p.floats(info+0x20,3)==center
        assert p.floats(info+0x5c,4)==((9.,12.,15.,4.) if mode=='node' else (3.5,8.5,15.,4.))
    f.data=b'';f.position=0
    assert f.call(0x438e20,this=serializer+0x10,args=(f.stream,info))&255==1
    assert f.position==len(f.data) and not f.errors
    captured.update(written=f.data.hex(),referenceWrites=writes,writerInstructions=sum(p.visits.values()))
    if mode=='clone':f.call(0x6d7db0)
    cleanup(f,objects)
    report=dict(kind='original-pc-collision-core',status='passed',profile='micro',
        limits=dict(arenaBytes=p.arena_size,instructionsPerCall=100000,microsecondsPerCall=2000000,processSeconds=30),
        scope='Metadata/field bodies/ownership/node bounds only. Explicit leaf reference fixture; no collision queries or scene broadphase.',
        probeSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        sample=captured,releasedAllocations=len(f.freed),arenaReservedBytes=p.allocated,seconds=time.monotonic()-start)
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report),flush=True)
    return 0

def obb_case(output):
    f=PCWriteBytesFixture();p=f.p;obj=f.call(0x4879c0);serializer=f.call(0x439a50)
    def capture(obj):
        return dict(position=p.floats(obj+0x4c,3),size=p.floats(obj+0x58,3),
            orientation=p.floats(obj+0x28,9),sphere=p.floats(obj+0x18,4))
    defaults=capture(obj)
    f.data=field(0,struct.pack('<3f',1,2,3))+field(1,struct.pack('<3f',2,4,4))+field(2,struct.pack('<4f',*Q))+b'\0'
    original=f.data
    assert f.call(0x439ba0,this=serializer+0x10,args=(f.stream,obj))&255==1
    assert f.position==len(f.data) and not f.errors
    decoded=capture(obj)
    assert decoded==dict(position=(1.,2.,3.),size=(2.,4.,4.),
        orientation=(.5,.5,0.,-.5,.5,0.,0.,0.,1.),sphere=(1.,2.,3.,3.))
    f.data=b'';f.position=0
    assert f.call(0x439e70,this=serializer+0x10,args=(f.stream,obj))&255==1
    written=f.data
    f.call(0x52fd90,this=0x755588)
    manager=f.call(0x412540);p.put_uint(0x74e060,manager)
    copied=f.call(0x412be0,this=manager,args=(obj,));cloned=capture(copied)
    assert cloned==defaults,'OBB clone inherited Named copy leaves geometric defaults'
    f.call(0x6d7db0);cleanup(f,(obj,copied,serializer))
    report=dict(kind='original-pc-obb-core',status='passed',profile='micro',
        probeSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        input=original.hex(),written=written.hex(),defaults=defaults,decoded=decoded,cloned=cloned,
        releasedAllocations=len(f.freed),arenaReservedBytes=p.allocated,
        scope='Original OBB ctor/read/write/clone/destruct. Byte stream and CRT only; no collision query.')
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report),flush=True);return 0

def graph_case(source,output):
    # Fresh declared file profile: whole FFPS/FAT/reference traversal, three
    # objects, <=4KiB input,128KiB arena,1M instructions/8s call,30s outer.
    # The historical micro cap on an older Node graph is not retried/resumed.
    from probe_pc_san_file_profile import FileFixture
    from probe_pc_node_relationships import node_rtti
    from probe_pc_imported_skin import seed_preserved_collision_rtti
    from pc_loader_fixtures import empty_fat,empty_manager
    source=Path(source).resolve();source.relative_to(ROOT/'local-data/results')
    raw=source.read_bytes();assert 32<=len(raw)<=4096 and struct.unpack_from('<I',raw,28)[0]==3
    f=FileFixture(raw);p=f.p;node_rtti(f);seed_preserved_collision_rtti(f);f.call(0x6d38e0)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
    for kind,factory in ((0x695c0f65,0x4638f0),(0x47a97c0e,0x438960),(0x4da04889,0x439a50)):
        serializer=f.call(factory);f.call(0x422d90,this=manager,args=(kind,serializer,255,3))
    f.call(0x45adf0)
    root=f.call(0x422b50,this=manager,args=(f.stream,));instructions=sum(p.visits.values())
    assert root and not f.errors and f.position==len(raw)
    assert p.uint(root+0x6c)-p.uint(root+0x68)==4
    info=p.uint(p.uint(root+0x68));obb=p.uint(info+0x14)
    assert p.uint(info+0x10)==root and p.floats(info+0x20,3)==(8.,10.,12.)
    assert p.floats(obb+0x58,3)==(2.,4.,4.) and p.floats(info+0x5c,4)==(8.,10.,12.,3.)
    stages=(0x422b50,0x422940,0x4678b0,0x438a80,0x439ba0,0x421ed0,0x4651e0,0x486930)
    assert all(a in p.visits for a in stages)
    report=dict(kind='original-pc-collision-whole-file',status='passed',profile='file',
        inputPath=str(source.relative_to(ROOT)),inputSha256=hashlib.sha256(raw).hexdigest().upper(),
        probeSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        collision=state(f,info),obbSize=p.floats(obb+0x58,3),wholeLoadInstructions=instructions,
        visitedStages={f'{a:08X}':p.visits[a] for a in stages},
        limits=dict(inputBytes=4096,objects=3,arenaBytes=p.arena_size,instructionsPerCall=1000000,microsecondsPerCall=8000000,processSeconds=30),
        scope='C++-written Node/CollisionInfo/OBB loaded by original whole FFPS path; prepared RTTI/startup/byte stream/CRT, no body or reference seams.')
    f.call(p.uint(p.uint(root)),this=root,args=(1,));f.call(0x4228a0,this=manager)
    cleanup(f,())
    assert info in f.freed and obb in f.freed
    report.update(releasedAllocations=len(f.freed),arenaReservedBytes=p.allocated)
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report),flush=True);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
