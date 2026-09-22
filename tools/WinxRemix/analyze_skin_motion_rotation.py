"""Own same-frame rotation oracle: immutable Skin versus rigid transform."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from PIL import Image

parser=argparse.ArgumentParser()
parser.add_argument('run',type=Path);parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
run=args.run.resolve()
output=args.output.resolve();assert not output.exists()
def read(name):return json.loads((run/name).read_text(encoding='utf-8-sig'))
def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest().upper()
launch,execution,cleanup=read('launch.json'),read('exit.json'),read('cleanup.json')
assert launch['motionHistory'] and launch['rotationHistory'] and launch['subdivisions']==24 and launch['maximumBonesPerVertex']==2
assert not launch['halfResolution'] and not launch['signedWeights']
assert execution['pid']==launch['pid'] and execution['exitCode']==0
assert not any(execution[key] for key in ('timeout','memoryExceeded','systemPressure'))
assert cleanup['clientExitCode']==0 and not cleanup['cleanupFailed'] and not cleanup['remaining']
assert cleanup['servers'] and all(s['normalExit'] and s['exitCode']==0 for s in cleanup['servers'])
for source in read('build.json')['hashes']:assert digest(run/source['path'])==source['sha256'],source['path']
assert digest(run/'.trex/d3d9.dll')==launch['rendererSha256']
rows=[json.loads(line) for line in (run/'fixture.jsonl').read_text().splitlines()]
assert rows[-1]['event']=='complete' and rows[-1]['captures']==10 and rows[-1]['motionHistoryExperiment']
assert not any(r['event']=='error' for r in rows)
contracts=[r for r in rows if r['event']=='rotation_contract'];assert len(contracts)==1
contract=contracts[0]
assert contract['vertices']==325 and contract['subdivisions']==24 and contract['separationPixels']==288
assert contract['anglesRadians']==[.004,.007] and not contract['skinCategoryIgnoresMotion']
completion=[r for r in rows if r['event']=='deviceCompletion'];assert len(completion)==1 and completion[0]['polls']>0
assert completion[0]['milliseconds']<=10000
configuration={r['key']:r['value'] for r in rows if r['event']=='config'}
for key,value in {'rtx.upscalerType':'0','rtx.resolutionScale':'1','rtx.forceCameraJitter':'False',
                  'rtx.postfx.enableMotionBlur':'False','rtx.enableRayReconstruction':'False'}.items():assert configuration[key]==value
phases=[r for r in rows if r['event']=='rotation_phase']
assert [(r['signedProfile'],r['phase']) for r in phases]==[(s,p) for s in range(2) for p in range(5)]
names=[f'rotation-s{s}-p{p}.bmp' for s in range(2) for p in range(5)]
assert [r['file'] for r in phases]==names and [r['file'] for r in rows if r['event']=='capture']==names
results=[]
for phase in phases:
    image=Image.open(run/phase['file']).convert('RGB');assert image.size==(960,540)
    left=[];right=[];analytic=[]
    angle=phase['angle'];c=math.cos(2*angle);s=math.sin(2*angle)
    for y in range(258,290):
        for x in range(316,356):
            left.append(image.getpixel((x,y)));right.append(image.getpixel((x+288,y)))
            # The current local point is the perspective projection of z=5.
            # Previous local rotation is R(-2*angle) times the current point.
            # Screen-space origin includes the rigid instance translation.
            qx=x+.5-336;qy=289.2-(y+.5)
            dx=(1-c)*qx-s*qy;dy=-s*qx-(1-c)*qy
            analytic.append([255*min(1,abs(dx)),255*min(1,abs(dy)),0])
    paired=[abs(a[k]-b[k]) for a,b in zip(left,right) for k in range(3)]
    checks=dict(settled=phase['frames']>=90,pairedMaximum=max(paired)<=4,pairedMean=sum(paired)/len(paired)<=2)
    summary={}
    if phase['phase']==0:
        checks['albedoVisible']=all(min(p)>=200 for pixels in (left,right) for p in pixels)
    elif phase['phase'] in (1,4):
        checks['returnedToZero']=all(max(p)<=3 for pixels in (left,right) for p in pixels)
        checks['zeroAngle']=angle==0
    else:
        expectedAngle=.004 if phase['phase']==2 else .007
        checks['angleMatches']=abs(abs(angle)-expectedAngle)<1e-8
        expectedRanges=[max(v[k] for v in analytic)-min(v[k] for v in analytic) for k in range(3)]
        errors=[];ranges=[]
        for pixels in (left,right):
            residual=[abs(actual[k]-expected[k]) for actual,expected in zip(pixels,analytic) for k in range(3)]
            errors.append(dict(maximum=max(residual),mean=sum(residual)/len(residual)))
            ranges.append([max(v[k] for v in pixels)-min(v[k] for v in pixels) for k in range(3)])
        checks['analyticMotion']=all(e['maximum']<=4 and e['mean']<=1.5 for e in errors)
        checks['spatialGradient']=all(r[0]>=16 and r[1]>=16 for r in ranges)
        checks['unusedChannelZero']=all(p[2]<=3 for pixels in (left,right) for p in pixels)
        summary=dict(analyticErrors=errors,actualRanges=ranges,expectedRanges=expectedRanges)
    results.append(dict(**phase,checks=checks,passed=all(checks.values()),pairedMaximum=max(paired),
                        pairedMean=sum(paired)/len(paired),**summary))
files=['launch.json','exit.json','cleanup.json','build.json','fixture.jsonl',*names]
report=dict(status='PASS' if all(r['passed'] for r in results) else 'FAIL',run=str(run),phases=results,
    contract=contract,complete=rows[-1],deviceCompletion=completion[0],execution=execution,cleanup=cleanup,
    rendererSha256=launch['rendererSha256'],hashes={p:digest(run/p) for p in files},
    roi=[316,258,356,290],pixelSeparation=288,
    scope='B2 positive/signed sum-one source weights, identical Rz on both bones, one immutable325-vertex Skin mesh and one rigid reference per profile. Same-frame RGB8 per-pixel motion and analytic rotation field, no varying-bone deformation/camera/DLSS/TAA qualification.')
output.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(dict(status=report['status'],phases=len(results),maximumPairedDifference=max(r['pairedMaximum'] for r in results),
    failed=[r for r in results if not r['passed']]),indent=2))
assert report['status']=='PASS'
