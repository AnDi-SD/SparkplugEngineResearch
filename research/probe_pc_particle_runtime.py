"""Fresh original particle readers/Init in one bounded guest, explicit world inputs."""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_scene_file_profile import SceneFileFixture
from pc_particle_fixtures import install_particle_caps
from probe_pc_particle_loop_init import capture_pool,random_state
from compare_pc_particle_parameters import field

BASE=ROOT/'local-data/results/tools-core-cycle-20260911-1900/particle'
class Fixture(SceneFileFixture):
    guest_arena_size=262144
def main(name):
    out=(BASE/name).resolve();assert out.parent==BASE.resolve() and not out.exists();out.mkdir(parents=True)
    (out/'probe-source.py').write_bytes(Path(__file__).read_bytes())
    f=Fixture(b'');p=f.p;install_particle_caps(f)
    started=time.perf_counter();report=dict(status='started',cases=[],limits=dict(profile='file',instructionsPerCall=1000000,secondsPerCall=8,childSeconds=30,arenaBytes=262144),
        inputs='Explicit borrowed RenderNode POD world input, region/lifetime/rate fields and original mapped initial PRNG state; actual particle factory/reader/Init/producer/reset/caps/teardown execute',wholeGameClaim=False)
    initial=random_state(p)
    world=struct.pack('<15f',1,2,3,-2,3,4,0,1,0,-1,0,0,0,0,1)
    node=p.allocate(0xb0);p.mu.mem_write(node+0x74,world)
    cases=[('zero-world',7,(0,0,0),(0,180),1,1),('directed-local',9,(3,4,0),(-10,75),0,1),
        ('parallel-basis',5,(1,1,1),(0,60),1,1),('right-angle',6,(0,0,1),(90,90),1,1),
        ('exact256',256,(0,0,1),(0,1),1,1),('nonloop',6,(0,0,0),(0,180),1,0)]
    current=[]
    def observe(mu,address,size,user):
        if address==0x4b97f0:
            obj=p.uint(p.reg('ECX')+0x18);current[0]['pools'].append(dict(stage='before-render-init',**capture_pool(f,obj)))
        elif address==0x48c100:current[0]['samplerBatches'].append(p.uint(p.reg('ESP')+4))
    for at in (0x4b97f0,0x48c100):p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=at,end=at)
    try:
        for label,count,direction,angle,world_space,loop in cases:
            values={1:struct.pack('<3f',*direction),2:struct.pack('<2f',2,3),3:struct.pack('<2f',*angle),
                6:struct.pack('<2f',-1,1),7:bytes([loop]),8:bytes([world_space]),10:struct.pack('<f',count),
                13:struct.pack('<8f',1.25,-2.5,3.75,0,1,0,4,6)}
            wire=b'\0'+b''.join(field(k,v) for k,v in sorted(values.items()))+b'\0'
            row=dict(name=label,status='started',parametersFixtureHex=wire.hex(),pools=[],samplerBatches=[],renderNodeInput=dict(worldTransformHex=world.hex()),
                wholeFileAcceptance=False,teardownClaimed=False,syntheticParameterInput=True,syntheticWorldInput=True,syntheticSeed=False)
            report['cases'].append(row);current[:]=[row]
            p.put_uint(0x73fe8c,initial['index']);p.mu.mem_write(0x755658,bytes.fromhex(initial['stateHex']))
            obj=f.call(0x48d8d0);serializer=f.call(0x49bbb0);p.put_uint(obj+0x5c,node)
            f.data=wire;f.position=0;before=time.perf_counter()
            result=f.call(0x49bce0,this=serializer+0x10,args=(f.stream,obj))&255
            row.update(result=result,seconds=time.perf_counter()-before,instructions=sum(p.visits.values()))
            assert result==1 and f.position==len(wire) and not f.errors
            row['pools'].append(dict(stage='after-reader-return',**capture_pool(f,obj)))
            assert len(row['pools'])==2
            row['visited']={f'{a:08X}':p.visits[a] for a in (0x48c340,0x4b97f0,0x48d1c0,0x48c400,0x48c100,0x4132b0,0x4620c0)}
            for owned in (obj,serializer):f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
            row.update(status='captured-loop-init-return',teardownClaimed=True)
            (out/(label+'.json')).write_text(json.dumps(row,indent=2)+'\n')
        for address in (0x75db84,0x75db78,0x75526c,0x755264):
            owned=p.uint(address)
            if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
        f.clear_declarations();assert set(f.allocations)==set(f.freed)
        report.update(status='passed',releasedAllocations=len(f.freed))
    except Exception as error:
        report.update(status='failed',error=f'{type(error).__name__}: {error}',stop=f'{p.reg("EIP"):08X}',tail=[f'{a:08X}' for a in p.tail]);raise
    finally:
        report.update(seconds=time.perf_counter()-started,arenaUsedBytes=p.allocated,capsCalls=f.particle_caps_calls)
        paths={Path(__file__).resolve(),ROOT/'local-data/pc-pristine/WinxClub.exe'}
        for module in tuple(sys.modules.values()):
            path=getattr(module,'__file__',None)
            if path and Path(path).resolve().parent==ROOT/'research':paths.add(Path(path).resolve())
        report['fingerprints']={x.relative_to(ROOT).as_posix():hashlib.sha256(x.read_bytes()).hexdigest().upper() for x in sorted(paths)}
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:report[k] for k in ('status','seconds','error','arenaUsedBytes','releasedAllocations') if k in report}),flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
