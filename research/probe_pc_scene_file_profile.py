#!/usr/bin/env python3
"""Whole original PC mesh/material/Fog scene under declared startup and COM inputs.

The selected seven-entry RTTI tree is a fixture, not completed CRT startup.
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

def seed_scene_rtti(f):
    p=f.p
    records=[(0x755310,0x415352a1,0),(0x7555f8,0x44de07fd,0x755310),
        (0x75dd88,0x695c0f65,0x7555f8),(0x75e150,0x603625d0,0x75dd88),
        (0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030),
        (0x75cf48,0x7ac95aec,0x755310),(0x75d548,0x6160348b,0x75dfd0),
        (0x75dfd0,0x5c0314c5,0x755310),(0x7630e8,0x797b39ec,0x75dfd0),
        (0x75ffa8,0x234c576b,0x75df70),(0x75df70,0x7f577c6d,0x755310),
        (0x75d428,0x33c34cf0,0x75e090),(0x763150,0x193b2671,0x763b40),
        (0x763b40,0x67974a9c,0x75e090),(0x75e090,0x3f077b6c,0x7603a0),
        (0x7603a0,0x46f043fe,0x7555f8)]
    for rec,identity,parent in records:p.put_uint(rec,identity);p.put_uint(rec+0x48,parent)
    targets=[(0x695c0f65,0x75dd88,0x421e20),(0x603625d0,0x75e150,0x425520),
        (0x763277db,0x760cf8,0x479ed0),(0x6160348b,0x75d548,0x41a390),
        (0x7ac95aec,0x75cf48,0x419e90),(0x33c34cf0,0x75d428,0),
        (0x234c576b,0x75ffa8,0x460e50)]
    tree=p.allocate(32);head=p.allocate(24);targets.sort()
    def build(rows,parent):
        if not rows:return head
        mid=len(rows)//2;identity,record,factory=rows[mid];node=p.allocate(24)
        p.put_uint(node+4,parent);p.put_uint(node+12,identity);p.put_uint(node+16,record)
        p.mu.mem_write(node+20,b'\x01\x00');p.put_uint(record+0x4c,factory)
        p.put_uint(node,build(rows[:mid],node));p.put_uint(node+8,build(rows[mid+1:],node));return node
    root=build(targets,head);left=right=root
    while p.uint(left)!=head:left=p.uint(left)
    while p.uint(right+8)!=head:right=p.uint(right+8)
    p.put_uint(head,left);p.put_uint(head+4,root);p.put_uint(head+8,right);p.mu.mem_write(head+20,b'\x01\x01')
    p.put_uint(tree+0x18,head);p.put_uint(tree+0x1c,len(targets));p.put_uint(0x755378,tree)
    for identity,_,_ in targets:assert f.call(0x4143f0,this=tree,args=(identity,))&255

checks=0
def check(condition,label):
    global checks
    checks+=1
    if not condition:raise AssertionError(label)

def capture_scene(f,root,entries):
    p=f.p;identities={};objects={}
    for _,_,_,obj in entries:
        check(obj in f.allocations,'published object is an actual native allocation')
        record=f.call(p.uint(p.uint(obj)+0x10),this=obj);identity=p.uint(record)
        check(identity not in objects,'directed capture requires distinct runtime classes')
        identities[obj]=identity;objects[identity]=obj
    def identity(obj):
        check(not obj or obj in identities,'graph edge refers to a published object')
        return identities.get(obj,0)
    def read(obj,offset,size):return bytes(p.mu.mem_read(obj+offset,size))
    captured={'rootClass':identity(root),'objects':{}}
    for kind,obj in objects.items():
        named=f.call(0x408370,this=obj,args=(0x44de07fd,))&255
        name=p.uint(obj+0x10) if named else 0
        row={'nameHex':cstring(p,name+9).hex() if name else '', 'stateHex':'','edges':[],'buffers':[],'layers':[]}
        if kind in (0x695c0f65,0x603625d0):
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
        elif kind==0x763277db:
            state=read(obj,0x18,1)+read(obj,0x1c,4)+read(obj,0x5c,4)
            row['edges']=[identity(p.uint(obj+a)) for a in (0x20,0x24,0x58)]
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
                row['layers'].append(layer_bytes.hex())
        elif kind==0x7ac95aec:state=read(obj,0x14,20)
        elif kind==0x193b2671:
            state=b''.join(read(obj,a,4) for a in (0x44,0x48,0x4c,0x60,0x64,0x70,0x74,0x78,0x7c))
            for a in (0x54,0x58):
                buffer=f.buffers[p.uint(p.uint(obj+a)+0x10)]
                row['buffers'].append(bytes(p.mu.mem_read(buffer['data'],buffer['size'])).hex())
            check(len(f.declaration_arrays)==1,'one shared native declaration')
            row['buffers'].append(f.declaration_arrays[0].hex())
        else:raise ValueError('Uncaptured native runtime class')
        row['stateHex']=state.hex();captured['objects'][str(kind)]=row
    return captured

def main():
    path=ROOT/'local-data/pc-pristine/Media/Menus/logo_screen.smo';raw=path.read_bytes()
    check(len(raw)==703 and hashlib.sha256(raw).hexdigest().upper()=='DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7','unchanged selected corpus SHA256')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSceneSerializationTests.exe'
    result=subprocess.run([str(binary),'--asset-file',str(path)],capture_output=True,timeout=10,check=True)
    check(len(result.stdout)<=262144,'bounded source capture')
    expected=json.loads(result.stdout)
    f=SceneFileFixture(raw);p=f.p;f.call(0x6d38e0);seed_scene_rtti(f)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
    for kind,factory,platform in [(0x695c0f65,0x4638f0,255),(0x603625d0,0x469040,255),
        (0x763277db,0x4934c0,255),(0x6160348b,0x42f690,255),(0x7ac95aec,0x43b830,255),(0x33c34cf0,0x4297c0,2)]:
        serializer=f.call(factory);f.call(0x422d90,this=manager,args=(kind,serializer,platform,1))
    f.call(0x45adf0);published=[]
    def observe(mu,address,size,user):
        if p.uint(fat+0x50):
            head=p.uint(fat+0x4c);at=p.uint(head);rows=[]
            while at!=head:
                check(len(rows)<16,'bounded FAT publication observer');entry=p.uint(at+8)
                rows.append([p.uint(entry),p.uint(entry+4),p.uint(entry+0x10),p.uint(entry+0x20)]);at=p.uint(at)
            published.append(rows)
    observer=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=0x466760,end=0x466760)
    report={'kind':'native-scene-whole-file-comparison','asset':'logo_screen.smo','executionProfile':'file',
        'inputSha256':hashlib.sha256(raw).hexdigest().upper(),'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),
        'arenaLimitBytes':p.arena_size,'maxAllocationBytes':f.max_allocation_size,'processTimeoutSeconds':30,'phase':'whole-load'}
    at=time.monotonic()
    try:
        root=f.call(0x422b50,this=manager,args=(f.stream,));p.mu.hook_del(observer)
        stages=(0x422260,0x466b90,0x465cd0,0x4aa870,0x422940,0x463a70,0x4938f0,0x429a40,0x43b910,0x466760)
        report.update(wholeLoadInstructions=sum(p.visits.values()),wholeLoadSeconds=time.monotonic()-at,
            executionLimits=p.last_execution_limits,visitedStages={f'{a:08X}':p.visits[a] for a in stages})
        for a in stages:check(p.visits[a]>0,f'actual whole-file stage {a:08X}')
        check(root and not f.errors and f.position==len(raw),'complete file consumed without diagnostics')
        check(len(published)==1 and len(published[0])==6,'six original objects published before actual FAT clear')
        check(p.uint(fat+0x28)==p.uint(fat+0x34)==p.uint(fat+0x50)==0,'whole loader clears FAT while graph stays alive')
        report['phase']='comparison';observed=capture_scene(f,root,published[0])
        directory=ROOT/'local-data/results/cycle-20260908-0700'
        (directory/'logo-native-scene-capture.json').write_text(json.dumps(observed,indent=2)+'\n')
        (directory/'logo-source-scene-capture.json').write_text(json.dumps(expected,indent=2)+'\n')
        check(observed==expected,'exact whole original/source scene state and edges')
        report['phase']='cleanup'
        combiners=[a for a,s in f.allocations.items() if a not in f.freed and s==0x2c and p.uint(a)==0x6ef294]
        check(len(combiners)==1,'native normal hook retains one combiner for explicit fixture cleanup')
        f.call(p.uint(p.uint(root)),this=root,args=(1,))
        for obj in combiners:f.call(0x4a9f30,this=obj,args=(1,))
        f.clear_declarations();f.call(0x4228a0,this=manager)
        for address in (0x75db90,0x75db78,0x75526c,0x755264):
            obj=p.uint(address)
            if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
        check(all(b['refs']==0 and b['locks']==0 for b in f.buffers.values()),'all fixture COM buffers released/unlocked')
        check(set(f.allocations)==set(f.freed),'all native allocations released including explicit combiner cleanup')
        report.update(status='passed',nativeAssertions=checks,exactObjects=6,releasedAllocations=len(f.freed),
            stateBytes=sum(len(r['stateHex'])//2 for r in observed['objects'].values()),
            bufferAndDeclarationBytes=sum(sum(len(b)//2 for b in r['buffers']) for r in observed['objects'].values()),
            materialLayerBytes=sum(sum(len(b)//2 for b in r['layers']) for r in observed['objects'].values()))
    except (AssertionError,ValueError) as error:
        report.update(status='stopped' if p.reg('EIP')!=0x32000000 else 'failed',error=str(error),
            failureInstructions=sum(p.visits.values()),stoppedIp=f'{p.reg("EIP"):08X}',fileCursor=f.position)
        raise
    finally:
        report.update(arenaReservedBytes=p.allocated,elapsedSeconds=time.monotonic()-at)
        (ROOT/'local-data/results/cycle-20260908-0700/cp103-scene-file.json').write_text(json.dumps(report,indent=2)+'\n')
        print('CAPTURE',json.dumps(report,sort_keys=True),flush=True)
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__)))

