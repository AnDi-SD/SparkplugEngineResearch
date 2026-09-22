"""Camera-only previous/current projection oracle, independently at each pixel."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from PIL import Image

parser=argparse.ArgumentParser();parser.add_argument('run',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
root=Path(__file__).resolve().parents[2];run=args.run.resolve()
output=args.output.resolve();assert not output.exists()
def read(name):return json.loads((run/name).read_text(encoding='utf-8-sig'))
def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest().upper()
launch,execution,cleanup=read('launch.json'),read('exit.json'),read('cleanup.json')
assert launch['motionHistory'] and launch['cameraHistory'] and launch['subdivisions']==24 and launch['maximumBonesPerVertex']==2
assert not launch['halfResolution'] and not launch['signedWeights']
assert execution['pid']==launch['pid'] and execution['exitCode']==0
assert not any(execution[key] for key in ('timeout','memoryExceeded','systemPressure'))
assert cleanup['clientExitCode']==0 and not cleanup['cleanupFailed'] and not cleanup['remaining']
assert cleanup['servers'] and all(s['normalExit'] and s['exitCode']==0 for s in cleanup['servers'])
for source in read('build.json')['hashes']:assert digest(run/source['path'])==source['sha256'],source['path']
assert digest(run/'.trex/d3d9.dll')==launch['rendererSha256']
rows=[json.loads(line) for line in (run/'fixture.jsonl').read_text().splitlines()]
assert rows[-1]['event']=='complete' and rows[-1]['captures']==12 and rows[-1]['motionHistoryExperiment']
assert not any(r['event']=='error' for r in rows)
contracts=[r for r in rows if r['event']=='camera_history_contract'];assert len(contracts)==1
contract=contracts[0]
assert contract['vertices']==325 and contract['subdivisions']==24 and contract['separationPixels']==288
assert contract['cameraXAmplitude']==.00125 and contract['cameraAngleAmplitude']==.00125 and contract['cameraZAmplitude']==.004
assert contract['sourceZ']==5 and not contract['skinCategoryIgnoresMotion']
assert contract.get('cameraBeforeWitness') is True
completion=[r for r in rows if r['event']=='deviceCompletion'];assert len(completion)==1 and completion[0]['polls']>0
assert completion[0]['milliseconds']<=10000
configuration={r['key']:r['value'] for r in rows if r['event']=='config'}
for key,value in {'rtx.upscalerType':'0','rtx.resolutionScale':'1','rtx.forceCameraJitter':'False',
                  'rtx.postfx.enableMotionBlur':'False','rtx.enableRayReconstruction':'False'}.items():assert configuration[key]==value
phases=[r for r in rows if r['event']=='camera_history_phase']
assert [(r['signedProfile'],r['phase']) for r in phases]==[(s,p) for s in range(2) for p in range(6)]
names=[f'camera-s{s}-p{p}.bmp' for s in range(2) for p in range(6)]
assert [r['file'] for r in phases]==names and [r['file'] for r in rows if r['event']=='capture']==names
results=[]
for phase in phases:
    image=Image.open(run/phase['file']).convert('RGB');assert image.size==(960,540)
    pose=(0,.00125,0) if phase['phase']==2 else (.00125,0,0) if phase['phase']==3 else (0,0,.004) if phase['phase']==4 else (0,0,0)
    assert all(abs(abs(phase[key])-want)<1e-8 for key,want in zip(('angle','cameraX','cameraZ'),pose))
    for control,x0 in enumerate((324,612)):
        pixels=[];expected=[]
        angle=phase['angle'];c=math.cos(2*angle);s=math.sin(2*angle)
        ratio=(5-phase['cameraZ'])/(5+phase['cameraZ'])
        for y in range(265,289):
            for x in range(x0,x0+24):
                pixels.append(image.getpixel((x,y)))
                qx=x+.5-480;qy=270-y-.5
                if phase['phase']==2:
                    dx=-192*phase['cameraX'];dy=0
                elif phase['phase']==3:
                    dx=(1-c)*qx-s*qy;dy=-s*qx-(1-c)*qy
                elif phase['phase']==4:
                    dx=(1-ratio)*qx;dy=-(1-ratio)*qy
                else:dx=dy=0
                expected.append([255*min(1,abs(dx)),255*min(1,abs(dy)),0])
        checks=dict(settled=phase['frames']>=90)
        summary={}
        if phase['phase']==0:checks['visible']=all(min(p)>=200 for p in pixels)
        elif phase['phase'] in (1,5):checks['returnedToZero']=all(max(p)<=3 for p in pixels)
        else:
            errors=[abs(actual[ch]-want[ch]) for actual,want in zip(pixels,expected) for ch in range(3)]
            ranges=[max(v[ch] for v in pixels)-min(v[ch] for v in pixels) for ch in range(3)]
            expected_ranges=[max(v[ch] for v in expected)-min(v[ch] for v in expected) for ch in range(3)]
            checks['analyticMotion']=max(errors)<=4 and sum(errors)/len(errors)<=1.5
            checks['unusedChannelZero']=all(p[2]<=3 for p in pixels)
            if phase['phase'] in (3,4):checks['spatialGradient']=max(expected_ranges)>4 and max(ranges)>=3
            summary=dict(analyticMaximum=max(errors),analyticMean=sum(errors)/len(errors),actualRanges=ranges,expectedRanges=expected_ranges)
        results.append(dict(**phase,control='skin' if control else 'rigid',checks=checks,passed=all(checks.values()),
            actualMean=[sum(p[ch] for p in pixels)/len(pixels) for ch in range(3)],
            expectedMean=[sum(p[ch] for p in expected)/len(expected) for ch in range(3)],**summary))
files=['launch.json','exit.json','cleanup.json','build.json','fixture.jsonl',*names]
passed=all(r['passed'] for r in results)
report=dict(status='PASS' if passed else 'FAIL',run=str(run),phases=results,contract=contract,complete=rows[-1],
    deviceCompletion=completion[0],execution=execution,cleanup=cleanup,rendererSha256=launch['rendererSha256'],
    hashes={p:digest(run/p) for p in files},
    scope='Camera-only X translation, Z rotation and Z translation, constant geometry/palette/instance transforms, positive/signed B2 sum-one. Independent RGB8 perspective motion oracle at each rigid/Skin pixel; camera jitter disabled. No combined deformation/camera, general 3D rotation, DLSS/TAA or full-game history qualification.')
output.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(dict(status=report['status'],phases=len(phases),controlChecks=len(results),
    maximumAnalyticError=max(r.get('analyticMaximum',0) for r in results),failed=[r for r in results if not r['passed']]),indent=2))
assert passed
