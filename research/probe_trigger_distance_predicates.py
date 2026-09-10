#!/usr/bin/env python3
"""Original trigger distance consumers, with explicitly restricted PS2 CPU profile."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/trigger-runtime-methods'
SPECS={
 'wxCT02OpenDoor':dict(pc=0x546d30,ps2=0x3a3960,end=0x3a39ac,full=True,kind='door',tolerance=100,threshold=20000),
 'wxGiantBoulderTrigger':dict(pc=0x545e50,ps2=0x3ad970,end=0x3ad9b8,full=True,kind='boulder',tolerance=0,threshold=160000),
 'wxCTCombinationLock':dict(pc=0x548c80,ps2=0x3a48b0,end=0x3a4940,kind='variable',tolerance=500),
 'wxPhoneTrigger':dict(pc=0x548c80,ps2=0x3b35b0,end=0x3b3640,kind='variable',tolerance=500),
 'wxChangeCharacterPlacement':dict(pc=0x541040,ps2=0x39bd00,end=0x39bd90,kind='variable',tolerance=300),
 'wxGenericSideQuest':dict(pc=0x591110,ps2=0x3d5df0,end=0x3d5e84,kind='sidequest',tolerance=100),
 'wxDragonFlowerTrigger':dict(pc=0x53bf20,ps2=0x3aa4c0,end=0x3aa54c,kind='dragon',tolerance=100),
 'wxGenericTrigger':dict(pc=0x590640,ps2=0x3ac890,end=0x3ac930,kind='generic',tolerance=100)}


def sha(value):return hashlib.sha256(value).hexdigest().upper()


def execute(c):
    pc=c['platform']=='pc';spec=SPECS[c['className']];kind=spec['kind']
    if pc:
        p=PcBlocks();base=p.allocate(0x9000);write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(spec['ps2'],spec['end']-spec['ps2']),(0x293210,0xd0),(0x423b58,0x28),(0x109af0,0x14)],profile='integer-squares')
        base=0x21000000;p.map(base,0x9000);p.map(0x22000000,4096);p.map(0x49f000,4096)
        write,read,put=p.write,p.read,p.put_uint;p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
    obj,record,profile,player,own_node,player_node,decoy=base,base+0x1000,base+0x2000,base+0x6000,base+0x7000,base+0x8000,base+0x1800
    write(base,b'\xa5'*0x9000);put(0x765ad4 if pc else 0x49fc7c,profile)
    put(profile+0x2b4,player);put(player+0x24,player_node)
    put(obj+0x18,decoy if kind=='sidequest' else own_node);put(obj+0x24,record);put(record+0x24,own_node if kind=='sidequest' else decoy)
    put(obj+(0x128 if pc else 0x134),c['threshold']);write(obj+(0x124 if pc else 0x130),bytes([c['enabled']]))
    if kind in ('door','boulder'):
        put(obj+(0x148 if pc else 0x160),own_node);put(obj+(0x14c if pc else 0x164),player_node)
    offset=0x74 if pc else 0x70
    write(own_node+offset,struct.pack('<3f',*c['point']));write(player_node+offset,struct.pack('<3f',*c['origin']));write(decoy+offset,struct.pack('<3f',1700,1700,1700))
    before=read(base,0x9000)
    delta=[a-b for a,b in zip(c['point'],c['origin'])]
    if kind=='boulder' or abs(delta[1])<spec['tolerance']:delta[1]=0
    square=sum(v*v for v in delta);enabled=kind!='generic' or bool(c['enabled']);expected=int(enabled and square<spec.get('threshold',c['threshold']))
    if pc:
        p.run(spec['pc'],this=obj);value=p.reg('EAX')&255
        result=dict(entry=f'{spec["pc"]:08X}',completion='whole original PC method',blocks=sum(p.visits.values()),executionProfile=p.execution_profile)
    else:
        p.reg('A0',obj);entry=spec['ps2'] if spec.get('full') else spec['ps2']+12;stop=p.RETURN if spec.get('full') else spec['end']
        code=[p.read(a,n) for a,n in p.ranges];result=p.run(entry,[stop],count=6000,timeout_us=500000);value=p.reg('V0')&0xffffffff
        assert len(result['accumulatorInterpretation'])==(3 if enabled else 0)
        assert [p.read(a,n) for a,n in p.ranges]==code
        result['completion']='whole original PS2 method via explicit CPU interpretation' if spec.get('full') else 'original own component after SQ frame;real full geometry helper;before register restore'
    assert value==expected,(value,expected,square)
    after=read(base,0x9000);assert after==before,'whole receiver/profile/player/node/record guard'
    result.update(returnValue=value,squaredDistance=square,threshold=spec.get('threshold',c['threshold']),comparison='strict less;integer-square inputs',guardedBytes=0x9000,beforeSha256=sha(before),afterSha256=sha(after),context='bounded borrowed records,explicit cached profile;no normal construction or cache-miss claim;PS2 integer-squares profile is not general EE FPU')
    return result


def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result required')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    rows=[c for c in json.loads((FOLDER/'distance-cases.json').read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='trigger-original-distance-consumers',status='running',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha((FOLDER/'distance-cases.json').read_bytes()),cases=[])
    started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in rows:
        case=dict(input=c);report['cases'].append(case);save()
        try:case.update(status='passed',**execute(c))
        except Exception as e:case.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save()
    print(json.dumps(dict(status=report['status'],cases=len(rows),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
