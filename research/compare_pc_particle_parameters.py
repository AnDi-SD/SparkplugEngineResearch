#!/usr/bin/env python3
"""Original complete non-looping particle read/write/lifetime versus compiled source."""
from pathlib import Path
import hashlib,json,struct,subprocess,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_scene_file_profile import SceneFileFixture
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_particle_fixtures import install_particle_caps,particle_state

def field(identity,data):
    fixed={1:1,2:2,4:3,8:4};code=fixed.get(len(data),5)
    return bytes([code*32+identity])+ (bytes([len(data)]) if code==5 else b'')+data

def specimen(mode,region):
    if mode=='default':return None
    if mode not in ('custom','defaults','threshold','direction-zero','direction-tiny','direction-negative') or not 12<=region<=18:
        raise ValueError('Explicit bounded particle specimen required')
    values={0:struct.pack('<6f',1,2,3,-4,-5,-6),1:struct.pack('<3f',3,4,0),
        2:struct.pack('<2f',2,3),3:struct.pack('<2f',-10,75),4:struct.pack('<2f',.25,2),
        5:struct.pack('<2I',0x12345678,0xdeadbeef),6:struct.pack('<2f',-1,.75),
        7:b'\0',8:b'\0',9:b'\3',10:struct.pack('<f',8),11:struct.pack('<f',42)}
    sizes={12:3,13:8,14:6,15:4,16:4,17:5,18:6}
    values[region]=struct.pack('<'+'f'*sizes[region],*(.25+i for i in range(sizes[region])))
    if mode=='defaults':values={k:v for k,v in values.items() if k in (7,region)}
    if mode=='threshold':values[0]=struct.pack('<6f',.001,0,1,0,0,1);values[1]=struct.pack('<3f',0,0,1)
    if mode.startswith('direction-'):
        values[1]=struct.pack('<3f',*( {'direction-zero':(0,0,0),'direction-tiny':(.001,0,0),'direction-negative':(-2,-3,-6)}[mode]))
    return field(2,struct.pack('<I',0))+field(3,struct.pack('<I',17))+b'\0'+b''.join(field(k,v) for k,v in sorted(values.items()))+b'\0'

def main(mode='custom',region=12,supported=False):
    payload=specimen(mode,region)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugParticleSerializationTests.exe'
    result=subprocess.run([str(binary),'--payload'],input=((payload.hex() if payload is not None else 'default')+'\n').encode(),capture_output=True,timeout=10)
    assert result.returncode==0,result.stderr.decode(errors='replace')
    expected=json.loads(result.stdout)
    f=SceneFileFixture(payload or b'');p=f.p;install_particle_caps(f,supported)
    f.messages=[];f.MaximumBytes=65536;f.write_calls=0;f.fail_write_call=None
    p.put_uint(p.uint(f.stream)+0x38,0x340600e0);p.seams[0x340600e0]=lambda p:PCWriteBytesFixture.write(f,p)
    p.seams[0x4135e0]=lambda p:PCWriteBytesFixture.trace(f,p)
    obj=f.call(0x48d8d0);serializer=f.call(0x49bbb0);p.put_uint(obj+0x5c,0)
    report={'kind':'native-particle-parameters-comparison','mode':mode,'regionField':region,'capsSupported':supported,
        'inputSha256':hashlib.sha256(payload or b'').hexdigest().upper(),'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),
        'executionProfile':'file','arenaLimitBytes':p.arena_size,'readerInstructions':0,'writerInstructions':0}
    started=time.monotonic()
    try:
        if payload is not None:
            assert f.call(0x49bce0,this=serializer+0x10,args=(f.stream,obj))&255==1
            report['readerInstructions']=sum(p.visits.values())
            assert f.position==len(payload) and not f.errors
            assert p.visits[0x48c340] and p.visits[0x4b97f0] and f.particle_caps_calls==1
            assert p.mu.mem_read(p.uint(obj+0x78)+0x1c,1)[0]==int(supported)
        observed={'stateHex':particle_state(f,obj).hex()}
        f.data=b'';f.position=0
        assert f.call(0x49c8d0,this=serializer+0x10,args=(f.stream,obj))&255==1
        report['writerInstructions']=sum(p.visits.values());observed['writerHex']=f.data.hex()
        assert observed==expected,{'observed':observed,'expected':expected}
        for owned in (obj,serializer):f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
        for address in (0x75db84,0x75db78,0x75526c,0x755264):
            owned=p.uint(address)
            if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
        f.clear_declarations();assert set(f.allocations)==set(f.freed)
        report.update(status='passed',stateBytes=len(observed['stateHex'])//2,writerBytes=len(f.data),
            stateSha256=hashlib.sha256(bytes.fromhex(observed['stateHex'])).hexdigest().upper(),
            writerSha256=hashlib.sha256(f.data).hexdigest().upper(),releasedAllocations=len(f.freed),capsCalls=f.particle_caps_calls)
    except (AssertionError,ValueError) as error:
        report.update(status='failed',error=str(error),stoppedIp=f'{p.reg("EIP"):08X}');raise
    finally:
        report.update(seconds=time.monotonic()-started,arenaReservedBytes=p.allocated)
        path=ROOT/'local-data/results/cycle-20260908-0700'/f'cp120-particle-{mode}-{region}-{int(supported)}.json'
        path.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2] if len(sys.argv)>2 else 'custom',int(sys.argv[3]) if len(sys.argv)>3 else 12,'--caps' in sys.argv))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
