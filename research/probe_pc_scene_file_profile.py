#!/usr/bin/env python3
"""Whole original PC mesh/material/Fog/light scenes under explicit startup/COM inputs.

The selected valid RTTI tree is a fixture, not completed CRT startup.
The original whole loader, DX batch, graph factories/readers and teardown run.
"""
from pathlib import Path
import sys,json,hashlib,time,struct,subprocess
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'research'))
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_dx_mesh_payload import MeshFixture
from pc_loader_fixtures import PCFileBytesFixture,empty_manager,empty_fat
from probe_pc_san_reader import ReaderFixture,cstring
from pc_stl_fixtures import install_char_traits
from probe_pc_texture_missing_mips import MissingMipFixture

class SceneFileFixture(MeshFixture):
    guest_execution_profile='file'
    guest_arena_size=131072
    def __init__(self,raw):
        super().__init__(raw);p=self.p
        p.put_uint(p.uint(self.stream)+0x3c,0x340600c0)
        p.seams[0x340600c0]=lambda p:PCFileBytesFixture.get_size(self,p)
        p.seams[0x4130f0]=lambda p:ReaderFixture.set_name(self,p)
        p.seams[0x4173e0]=lambda p:ReaderFixture.release_name(self,p)
        install_char_traits(p)

class CrystalSceneFileFixture(SceneFileFixture):
    guest_max_buffer_size=8192

def seed_scene_rtti(f,with_light=False,with_material=None,with_skin=False,with_texture=False):
    p=f.p
    records=[(0x755310,0x415352a1,0),(0x7555f8,0x44de07fd,0x755310),
        (0x75dd88,0x695c0f65,0x7555f8),(0x75e150,0x603625d0,0x75dd88),
        (0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030),
        (0x75cf48,0x7ac95aec,0x755310),(0x75d548,0x6160348b,0x75dfd0),
        (0x75dfd0,0x5c0314c5,0x755310),(0x7630e8,0x797b39ec,0x75dfd0),
        (0x75ffa8,0x234c576b,0x75df70),(0x75df70,0x7f577c6d,0x755310),
        (0x75d428,0x33c34cf0,0x75e090),(0x763150,0x193b2671,0x763b40),
        (0x763b40,0x67974a9c,0x75e090),(0x75e090,0x3f077b6c,0x7603a0),
        (0x7603a0,0x46f043fe,0x7555f8),(0x75e278,0x72444900,0x75dd88),
        (0x75d4e8,0x5e6402df,0x75e278),(0x7634b0,0x6b3e7baa,0x75e278),
        (0x760520,0x681f2043,0x760cf8),
        (0x75d488,0x78ea082b,0x75df10),(0x75df10,0x2f281e13,0x7555f8),(0x763210,0x3f3651b6,0x75df10)]
    for rec,identity,parent in records:p.put_uint(rec,identity);p.put_uint(rec+0x48,parent)
    targets=[(0x695c0f65,0x75dd88,0x421e20),(0x603625d0,0x75e150,0x425520),
        (0x763277db,0x760cf8,0x479ed0),(0x6160348b,0x75d548,0x41a390),
        (0x7ac95aec,0x75cf48,0x419e90),(0x33c34cf0,0x75d428,0),
        (0x234c576b,0x75ffa8,0x460e50)]
    if with_light:
        if not with_material:targets=[row for row in targets if row[0]!=0x6160348b]
        targets += [(0x5e6402df,0x75d4e8,0x41a330)]
        # The common mesh path publishes a freshly created runtime DXMesh to
        # the resource manager, which queries runtime (not only wire) RTTI.
        targets += [(0x193b2671,0x763150,0x4a9e80),(0x6b3e7baa,0x7634b0,0x4ac000)]
    if with_skin:
        targets += [(0x681f2043,0x760520,0x46a120)]
        if not with_light:targets += [(0x193b2671,0x763150,0x4a9e80)]
    if with_texture:targets += [(0x78ea082b,0x75d488,0x41a2d0),(0x3f3651b6,0x763210,0x4ab520)]
    tree=p.allocate(32);head=p.allocate(24);targets.sort()
    red_level=(len(targets)+1).bit_length()-1
    def build(rows,parent,depth=0):
        if not rows:return head
        mid=len(rows)//2;identity,record,factory=rows[mid];node=p.allocate(24)
        p.put_uint(node+4,parent);p.put_uint(node+12,identity);p.put_uint(node+16,record)
        p.mu.mem_write(node+20,bytes((int(depth!=red_level),0)));p.put_uint(record+0x4c,factory)
        p.put_uint(node,build(rows[:mid],node,depth+1));p.put_uint(node+8,build(rows[mid+1:],node,depth+1));return node
    root=build(targets,head);left=right=root
    while p.uint(left)!=head:left=p.uint(left)
    while p.uint(right+8)!=head:right=p.uint(right+8)
    p.put_uint(head,left);p.put_uint(head+4,root);p.put_uint(head+8,right);p.mu.mem_write(head+20,b'\x01\x01')
    p.put_uint(tree+0x18,head);p.put_uint(tree+0x1c,len(targets));p.put_uint(0x755378,tree)
    def validate(node,parent,lower=-1,upper=0x100000000):
        if node==head:return 1
        identity=p.uint(node+12);assert lower<identity<upper and p.uint(node+4)==parent
        left,right=p.uint(node),p.uint(node+8);black=bool(p.mu.mem_read(node+20,1)[0])
        if not black:assert all(child==head or p.mu.mem_read(child+20,1)[0] for child in (left,right))
        a,b=validate(left,node,lower,identity),validate(right,node,identity,upper);assert a==b
        return a+black
    assert p.mu.mem_read(root+20,1)[0];validate(root,head)
    for identity,_,_ in targets:assert f.call(0x4143f0,this=tree,args=(identity,))&255

checks=0
def check(condition,label):
    global checks
    checks+=1
    if not condition:raise AssertionError(label)

def capture_scene(f,root,entries,by_file_id=False):
    p=f.p;identities={};objects={};kinds={}
    for file_id,_,_,obj in sorted(entries):
        check(obj in f.allocations,'published object is an actual native allocation')
        record=f.call(p.uint(p.uint(obj)+0x10),this=obj);identity=p.uint(record)
        key=file_id if by_file_id else identity
        check(key not in objects,'capture requires distinct file IDs or selected runtime classes')
        identities.setdefault(obj,key);objects[key]=obj;kinds[obj]=identity
    def identity(obj):
        check(not obj or obj in identities,'graph edge refers to a published object')
        return identities.get(obj,0)
    def read(obj,offset,size):return bytes(p.mu.mem_read(obj+offset,size))
    captured={'rootClass':kinds[root],'objects':{}}
    if by_file_id:captured['rootID']=identity(root)
    for key,obj in objects.items():
        kind=kinds[obj]
        named=f.call(0x408370,this=obj,args=(0x44de07fd,))&255
        name=p.uint(obj+0x10) if named else 0
        row={'nameHex':cstring(p,name+9).hex() if name else '', 'stateHex':'','edges':[],'buffers':[],'layers':[]}
        if by_file_id:row['classID']=kind
        if kind in (0x695c0f65,0x603625d0,0x6b3e7baa):
            state=read(obj,0xb0,4)+b''.join(read(obj,a,n) for a,n in ((0x20,12),(0x30,12),(0x40,36),(0x74,12),(0x80,12),(0x8c,36)))
            head=p.uint(obj+0x18);at=p.uint(head)
            while at!=head:
                check(len(row['edges'])<32,'bounded child list');child=p.uint(at+8)
                check(p.uint(child+0x2c)==obj,'reciprocal child parent')
                row['edges'].append(identity(child));at=p.uint(at)
            if kind==0x603625d0:
                begin,end=p.uint(obj+0xbc),p.uint(obj+0xc0)
                check(0<=end-begin<=128 and (end-begin)%4==0,'bounded renderable vector')
                row['edges'] += [identity(p.uint(at)) for at in range(begin,end,4)]
            if kind==0x6b3e7baa:
                state+=read(obj,0xc0,20)+b''.join(read(obj,a,1) for a in (0xec,0xd4,0xed))
                state+=b''.join(read(obj,a,4) for a in (0xd8,0xe0,0xe4,0xe8))
        elif kind in (0x763277db,0x681f2043):
            state=read(obj,0x18,1)+read(obj,0x1c,4)+read(obj,0x5c,4)
            row['edges']=[identity(p.uint(obj+a)) for a in (0x20,0x24,0x58)]
            if kind==0x681f2043:
                count=p.uint(obj+0x64);check(count<=32,'bounded native skin palette')
                state+=read(obj,0x60,8)+read(p.uint(obj+0x6c),0,count*64)
                row['edges'] += [identity(p.uint(p.uint(obj+0x68)+4*i)) for i in range(count)]
        elif kind==0x797b39ec:
            state=read(obj,0x18,44)+read(obj,0x6d,1)+read(obj,0x78,68)+read(obj,0x48,4)
            count=p.uint(obj+0x48);check(count<=8,'bounded native passes')
            for i in range(count):
                owner=p.uint(obj+0x4c+4*i);n=p.uint(owner+0x14);check(n<=8,'bounded native layers')
                layer_bytes=read(owner,0x10,8)
                for j in range(n):
                    layer=p.uint(owner+0x18+4*j);record=f.call(p.uint(p.uint(layer)+0x10),this=layer)
                    texture=p.uint(layer+0x10)
                    layer_bytes += struct.pack('<I',p.uint(record))+read(texture,0x10,36)+read(texture,0x3c,36)+read(texture,0x60,1)
                    if p.uint(texture+0x34):row['edges'].append(identity(p.uint(texture+0x34)))
                row['layers'].append(layer_bytes.hex())
        elif kind==0x7ac95aec:state=read(obj,0x14,20)
        elif kind==0x193b2671:
            state=b''.join(read(obj,a,4) for a in (0x44,0x48,0x4c,0x60,0x64,0x70,0x74,0x78,0x7c))
            for a in (0x54,0x58):
                buffer=f.buffers[p.uint(p.uint(obj+a)+0x10)]
                row['buffers'].append(bytes(p.mu.mem_read(buffer['data'],buffer['size'])).hex())
            check(len(f.declaration_arrays)==1,'one shared native declaration')
            row['buffers'].append(f.declaration_arrays[0].hex())
        elif kind==0x3f3651b6:
            state=b''.join(read(obj,a,n) for a,n in ((0x18,4),(0x1c,1),(0x20,4),(0x24,1),(0x28,4),(0x2c,4)))
            io=f.texture_io;check(p.uint(obj+0x3c)==io.texture,'captured texture owns declared COM identity')
            for rec in io.levels:
                packed=b''.join(read(rec['pixels'],r*rec['pitch'],rec['row_bytes']) for r in range(rec['rows']))
                row['buffers'].append(packed.hex())
        else:raise ValueError('Uncaptured native runtime class')
        row['stateHex']=state.hex();captured['objects'][str(key)]=row
    return captured

def main(case='logo',by_file_id=False):
    cases={'logo':('Menus/logo_screen.smo',703,'DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7'),
        'bloom-projectile':('Characters/Bloom/bloom_projectile.smo',770,'BE5C62D8A9A00FCBB51E987C7C9FFBDC92433C0FA2BB81001E20A9A2419C928D'),
        'g-crystal':('SFX/g_crystal.smo',6264,'9ECB8CFEFADD7A30F1411F8235039FB07EA342BA13177B4060DE975158609A0A'),
        'droid-trail':('SFX/droid_trail.smo',3070,'4781A76774FD2F079AF853B1ECC7235FB0B743FFFE071B34F9D8ACEED9C7AEA8'),
        'loading':('Menus/loading.smo',2442,'0E8EB7A89E952CD0CF096AE4F5E3F1FE4D56BEC3696DB427567F0BB2BF04427E')}
    check(case in cases,'selected bounded whole scene case')
    relative,size,digest=cases[case];with_light=case in ('bloom-projectile','g-crystal')
    common_mesh=case in ('bloom-projectile','droid-trail');with_skin=case=='droid-trail'
    with_texture=case=='loading'
    by_file_id=by_file_id or case in ('g-crystal','droid-trail','loading')
    object_count={'g-crystal':26,'droid-trail':13,'loading':11}.get(case,6)
    path=ROOT/'local-data/pc-pristine/Media'/relative;raw=path.read_bytes()
    check(len(raw)==size and hashlib.sha256(raw).hexdigest().upper()==digest,'unchanged selected corpus SHA256')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSceneSerializationTests.exe'
    result=subprocess.run([str(binary),'--asset-file-ids' if by_file_id else '--asset-file',str(path)],capture_output=True,timeout=10)
    check(result.returncode==0,'source whole capture: '+result.stderr.decode('utf-8',errors='replace')[:2048])
    check(len(result.stdout)<=262144,'bounded source capture')
    expected=json.loads(result.stdout)
    f=(CrystalSceneFileFixture if case=='g-crystal' else SceneFileFixture)(raw)
    if with_texture:MissingMipFixture.install_on_scene(f)
    p=f.p;f.call(0x6d38e0);seed_scene_rtti(f,with_light,with_material=case!='bloom-projectile',with_skin=with_skin,with_texture=with_texture)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
    bindings=[(0x695c0f65,0x4638f0,255),(0x603625d0,0x469040,255),
        (0x763277db,0x4934c0,255),(0x6160348b,0x42f690,255),(0x7ac95aec,0x43b830,255),(0x33c34cf0,0x4297c0,2)]
    if common_mesh:
        bindings=[row for row in bindings if row[0]!=0x33c34cf0 and (case!='bloom-projectile' or row[0]!=0x6160348b)]
        bindings += [(0x33c34cf0,0x42aef0,1)]
    if with_light:bindings += [(0x5e6402df,0x43ffd0,255)]
    if with_skin:bindings += [(0x681f2043,0x490c50,255)]
    if with_texture:bindings += [(0x78ea082b,0x42b660,6)]
    for kind,factory,platform in bindings:
        serializer=f.call(factory);f.call(0x422d90,this=manager,args=(kind,serializer,platform,1))
    f.call(0x45adf0);published=[]
    def observe(mu,address,size,user):
        if p.uint(fat+0x50):
            head=p.uint(fat+0x4c);at=p.uint(head);rows=[]
            while at!=head:
                check(len(rows)<32,'bounded FAT publication observer');entry=p.uint(at+8)
                # Native FAT entry +0 is the external file ID; +4 is the
                # object's own ID. All inline entries have external file ID0.
                rows.append([p.uint(entry+4),p.uint(entry+0x10),p.uint(entry+0x14),p.uint(entry+0x20)]);at=p.uint(at)
            published.append(rows)
    observer=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=0x466760,end=0x466760)
    report={'kind':'native-scene-whole-file-comparison','asset':path.name,'executionProfile':'file',
        'inputSha256':hashlib.sha256(raw).hexdigest().upper(),'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),
        'arenaLimitBytes':p.arena_size,'maxAllocationBytes':f.max_allocation_size,'maxCOMBufferBytes':f.max_buffer_size,
        'identityMode':'file-id' if by_file_id else 'runtime-class','processTimeoutSeconds':30,'phase':'whole-load'}
    if with_texture:report.update(maxTextureSurfaceBytes=2048,maxTextureLevels=5,textureDimensions=[16,16])
    at=time.monotonic()
    try:
        root=f.call(0x422b50,this=manager,args=(f.stream,));p.mu.hook_del(observer)
        stages=(0x422260,0x466b90,0x465cd0,0x4aa870,0x422940,0x463a70,0x4938f0,0x429a40,0x43b910,0x466760)
        if common_mesh:
            stages=tuple(a for a in stages if a not in (0x4aa870,0x429a40))
            stages+=(0x42afd0,0x42b420)
        if with_light:stages+=(0x4400b0,0x440640,0x4b58d0,0x421a60)
        if with_skin:stages+=(0x491170,0x46a120,0x421a60)
        if with_texture:stages+=(0x42dd10,0x42c640,0x4abba0,0x4ab030,0x61039a,0x60fdb4)
        report.update(wholeLoadInstructions=sum(p.visits.values()),wholeLoadSeconds=time.monotonic()-at,
            executionLimits=p.last_execution_limits,visitedStages={f'{a:08X}':p.visits[a] for a in stages})
        for a in stages:check(p.visits[a]>0,f'actual whole-file stage {a:08X}')
        check(root and not f.errors and f.position==len(raw),'complete file consumed without diagnostics')
        check(len(published)==1 and len(published[0])==object_count,'all expected objects published before actual FAT clear')
        check(p.uint(fat+0x28)==p.uint(fat+0x34)==p.uint(fat+0x50)==0,'whole loader clears FAT while graph stays alive')
        report['phase']='comparison';observed=capture_scene(f,root,published[0],by_file_id)
        directory=ROOT/'local-data/results/cycle-20260908-0700'
        suffix='ids' if by_file_id else 'class'
        (directory/f'cp109-{case}-{suffix}-native.json').write_text(json.dumps(observed,indent=2)+'\n')
        (directory/f'cp109-{case}-{suffix}-source.json').write_text(json.dumps(expected,indent=2)+'\n')
        check(observed==expected,'exact whole original/source scene state and edges')
        report['phase']='cleanup'
        combiners=[a for a,s in f.allocations.items() if a not in f.freed and s==0x2c and p.uint(a)==0x6ef294]
        check(len(combiners)==(0 if common_mesh else 1),'native common path has no batch; PC hook retains one combiner')
        f.call(p.uint(p.uint(root)),this=root,args=(1,))
        for obj in combiners:f.call(0x4a9f30,this=obj,args=(1,))
        f.clear_declarations();f.call(0x4228a0,this=manager)
        for address in (0x75db90,0x75db78,0x75526c,0x755264):
            obj=p.uint(address)
            if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
        check(all(b['refs']==0 and b['locks']==0 for b in f.buffers.values()),'all fixture COM buffers released/unlocked')
        if with_texture:
            io=f.texture_io
            check(io.device_refs==1 and io.texture_refs==io.surface_refs==0 and not io.locked,'whole graph releases texture and device acquisition')
            check(all(rec['refs']==0 and not rec['locked'] for rec in io.levels),'all generated mip surfaces released')
        check(set(f.allocations)==set(f.freed),'all native allocations released including explicit combiner cleanup')
        report.update(status='passed',nativeAssertions=checks,exactObjects=object_count,releasedAllocations=len(f.freed),
            stateBytes=sum(len(r['stateHex'])//2 for r in observed['objects'].values()),
            bufferAndDeclarationBytes=sum(sum(len(b)//2 for b in r['buffers']) for r in observed['objects'].values()),
            materialLayerBytes=sum(sum(len(b)//2 for b in r['layers']) for r in observed['objects'].values()))
    except (AssertionError,ValueError) as error:
        report.update(status='stopped' if p.reg('EIP')!=0x32000000 else 'failed',error=str(error),
            failureInstructions=sum(p.visits.values()),stoppedIp=f'{p.reg("EIP"):08X}',fileCursor=f.position)
        raise
    finally:
        report.update(arenaReservedBytes=p.allocated,elapsedSeconds=time.monotonic()-at)
        name=f'cp109-{case}-'+('ids' if by_file_id else 'class')+'.json'
        (ROOT/'local-data/results/cycle-20260908-0700'/name).write_text(json.dumps(report,indent=2)+'\n')
        print('CAPTURE',json.dumps(report,sort_keys=True),flush=True)
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2] if len(sys.argv)>2 else 'logo','--file-ids' in sys.argv[3:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))

