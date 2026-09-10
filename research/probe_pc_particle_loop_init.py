#!/usr/bin/env python3
"""One unchanged PC looping-particle scene; actual reader/Init/producer capture.

The named 64x64 COM input is a bounded host fixture, not renderer recovery.
Engine factories/readers/producer/samplers/PRNG execute without success seams.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_scene_file_profile import SceneFileFixture,seed_scene_rtti
from probe_pc_texture_missing_mips import MissingMipFixture
from pc_compact_texture_device import install_texture_device
from pc_particle_fixtures import install_particle_caps,particle_state
from pc_loader_fixtures import empty_fat,empty_manager

ASSET=ROOT/'local-data/pc-pristine/Media/Menus/bg_particles.smo'
DIGEST='8AED1BD2A6A308C5C8AE38EF7AEF5CB96190B4B691DAD181703025AB8D2892A4'
OUT=ROOT/'local-data/results/tools-core-cycle-20260910-0730/particle-loop-init'

def sha(data):return hashlib.sha256(data).hexdigest().upper()

def particle_surface_profile(dimensions):
    if dimensions!=(64,64):raise ValueError('Only the indexed bg_particle 64x64 surface is declared')
    return dict(dimensions=dimensions,max_levels=7,max_surface_bytes=(64*4+4)*64)

def install_particle_texture(f):
    profile=particle_surface_profile((64,64))
    install_texture_device(f,256,fixture_class=MissingMipFixture)
    io=f.texture_io
    for name,value in profile.items():setattr(io,name,value)
    io.levels=[];io.surfaces={};io.install_external_inputs();f.texture_ios=[io]

class Fixture(SceneFileFixture):
    def __init__(self,raw):
        self.calls=[]
        super().__init__(raw)
    def call(self,entry,this=0,args=()):
        started=time.monotonic()
        try:return super().call(entry,this=this,args=args)
        finally:self.calls.append(dict(entry=f'{entry:08X}',instructions=sum(self.p.visits.values()),
            seconds=time.monotonic()-started,**self.p.last_execution_limits))

def random_state(p):
    return dict(index=p.uint(0x73fe8c),stateHex=bytes(p.mu.mem_read(0x755658,624*4)).hex())

def capture_pool(f,obj):
    p=f.p;vb=p.uint(obj+0x88);count=p.uint(vb+0x1c)
    stride=struct.unpack('<H',p.mu.mem_read(vb+0x18,2))[0]*4
    assert 1<=count<=1024 and stride==32
    storage=p.uint(vb+0x54);nodes=p.uint(obj+0x8c)
    assert storage in f.allocations and nodes in f.allocations
    assert f.allocations[storage]>=count*stride and f.allocations[nodes]>=count*12
    def node_index(pointer):
        assert nodes<=pointer<nodes+count*12 and (pointer-nodes)%12==0
        return (pointer-nodes)//12
    links=[]
    for i in range(count):
        data,previous,next_=struct.unpack('<III',p.mu.mem_read(nodes+i*12,12))
        assert storage<=data<storage+count*stride and (data-storage)%stride==0
        links.append([(data-storage)//stride,node_index(previous),node_index(next_)])
    active,free,delta,clock,accumulator=struct.unpack('<IIIII',p.mu.mem_read(obj+0x98,20))
    assert active+free==count
    return dict(object=f'{obj:08X}',parametersHex=particle_state(f,obj).hex(),count=count,stride=stride,
        active=active,free=free,deltaBits=delta,clockBits=clock,accumulatorBits=accumulator,
        first=node_index(p.uint(obj+0x90)),boundary=node_index(p.uint(obj+0x94)),links=links,
        recordsHex=bytes(p.mu.mem_read(storage,count*stride)).hex(),random=random_state(p),
        renderNode=f'{p.uint(obj+0x5c):08X}')

def main(run_name='original-run2'):
    assert run_name in ('original-run2','original-run3')
    raw=ASSET.read_bytes();assert len(raw)==17048 and sha(raw)==DIGEST
    report=dict(kind='original-particle-loop-init',status='started',asset=ASSET.relative_to(ROOT).as_posix(),
        inputSha256=DIGEST,inputBytes=len(raw),seed=5489,wholeSceneExecuted=False,
        limits=dict(profile='file',instructionsPerCall=1000000,microsecondsPerCall=8000000,
            arenaBytes=131072,childSeconds=30,workers=1),
        declaredTextureProfile=particle_surface_profile((64,64)),
        inputs=['unchanged whole SMO','existing selected valid RTTI/FAT/manager fixture, not CRT startup',
            'existing external byte-stream/heap/name/diagnostic/COM contracts',
            'particle caps supported=false; original renderer query body executes',
            'original MT Seed5489 before whole loader; no particle/sampler/transform seams'],
        observations=[],pools=[],samplerBatches=[])
    f=None;started=time.monotonic();hooks=[]
    try:
        f=Fixture(raw);p=f.p;install_particle_texture(f);install_particle_caps(f)
        f.call(0x6d38e0)
        seed_scene_rtti(f,with_material=True,with_texture=True,with_particle=True)
        fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
        bindings=[(0x695c0f65,0x4638f0,255),(0x603625d0,0x469040,255),
            (0x763277db,0x4934c0,255),(0x6160348b,0x42f690,255),
            (0x7ac95aec,0x43b830,255),(0x33c34cf0,0x4297c0,2),
            (0x5afa1a4f,0x49bbb0,255)]
        for kind,factory,platform in bindings:
            serializer=f.call(factory);f.call(0x422d90,this=manager,args=(kind,serializer,platform,1))
        # Original common+PC2 texture registrations. This file declares
        # platform1; the earlier pickup-only harness registered only PC2.
        f.call(0x6d1880);f.call(0x6d1940)
        f.call(0x45adf0);p.run(0x413270,args=(5489,),callee_pop=False)
        published=[]
        def observe(mu,address,size,user):
            if address==0x466760:
                if p.uint(fat+0x50):
                    head=p.uint(fat+0x4c);at=p.uint(head);rows=[]
                    while at!=head:
                        assert len(rows)<6
                        entry=p.uint(at+8);rows.append([p.uint(entry+4),p.uint(entry+0x10),p.uint(entry+0x20)])
                        at=p.uint(at)
                    published.append(rows)
                return
            report['observations'].append(dict(address=f'{address:08X}',cursor=f.position))
            if address==0x48d1c0:
                obj=p.reg('ECX');report['pools'].append(dict(stage='before-loop-init',**capture_pool(f,obj)))
                rn=p.uint(obj+0x5c)
                report['renderNodeInput']=dict(pointer=f'{rn:08X}',vtable=f'{p.uint(rn):08X}',
                    worldTransformHex=bytes(p.mu.mem_read(rn+0x74,60)).hex())
                return_address=p.uint(p.reg('ESP'))
                def after(mu,address,size,user):
                    report['pools'].append(dict(stage='after-loop-init',**capture_pool(f,obj)))
                hooks.append(p.mu.hook_add(p.uc.UC_HOOK_CODE,after,begin=return_address,end=return_address))
            if address==0x48c100:
                report['samplerBatches'].append(dict(count=p.uint(p.reg('ESP')+4),randomIndex=p.uint(0x73fe8c)))
        for address in (0x466760,0x49bce0,0x48c340,0x4b97f0,0x48d1c0,0x48c400,0x48c100):
            hooks.append(p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=address,end=address))
        root=f.call(0x422b50,this=manager,args=(f.stream,))
        report['loadInstructions']=sum(p.visits.values())
        report['visited']={f'{a:08X}':p.visits[a] for a in (0x48d8d0,0x49bce0,0x48c340,0x48d1c0,0x48c400,0x48c100,0x4132b0,0x420350,0x4620c0,0x461d70)}
        assert root and not f.errors and f.position==len(raw) and f.data==raw
        assert len(published)==1 and len(published[0])==6
        report['published']=published[0];report['wholeSceneExecuted']=True
        assert len(report['pools'])==2 and report['pools'][-1]['active']>0
        for hook in hooks:p.mu.hook_del(hook)
        f.call(p.uint(p.uint(root)),this=root,args=(1,))
        f.clear_declarations();f.call(0x4228a0,this=manager)
        for address in (0x75db84,0x75db90,0x75db78,0x75526c,0x755264):
            obj=p.uint(address)
            if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
        assert set(f.allocations)==set(f.freed)
        io=f.texture_io
        assert io.device_refs==1 and io.texture_refs==io.surface_refs==0 and not io.locked
        report.update(status='captured',releasedAllocations=len(f.freed))
    except Exception as error:
        report.update(status='blocked',error=f'{type(error).__name__}: {error}')
        if f:report['stop']=dict(ip=f'{f.p.reg("EIP"):08X}',cursor=f.position,reason=f.p.reason,
            instructions=sum(f.p.visits.values()),tail=[f'{a:08X}' for a in f.p.tail],
            registers={name:f'{f.p.reg(name):08X}' for name in ('EAX','EBX','ECX','EDX','ESI','EDI','EBP','ESP')})
        raise
    finally:
        report['seconds']=time.monotonic()-started
        if f:report.update(calls=f.calls,arenaUsedBytes=f.p.allocated,allocations=len(f.allocations),freed=len(f.freed))
        paths={Path(__file__).resolve(),ASSET,ROOT/'local-data/pc-pristine/WinxClub.exe'}
        for module in tuple(sys.modules.values()):
            path=getattr(module,'__file__',None)
            if path and Path(path).resolve().parent==ROOT/'research':paths.add(Path(path).resolve())
        report['fingerprints']={x.relative_to(ROOT).as_posix():sha(x.read_bytes()) for x in sorted(paths)}
        OUT.mkdir(parents=True,exist_ok=True)
        (OUT/(run_name+'.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        (OUT/(run_name+'-probe.py')).write_bytes(Path(__file__).read_bytes())
        print(json.dumps({k:report[k] for k in ('status','error','seconds','stop','loadInstructions','arenaUsedBytes','releasedAllocations') if k in report}),flush=True)
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--profile-guards']:
        assert particle_surface_profile((64,64))['max_surface_bytes']==16640
        for bad in ((32,32),(64,65),(128,128)):
            try:particle_surface_profile(bad)
            except ValueError:pass
            else:raise AssertionError('profile accepted undeclared surface')
        print('PASS 4 particle64 profile guards')
    elif sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2] if len(sys.argv)>2 else 'original-run2'))
    else:raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
