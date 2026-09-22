"""Independent per-pixel affine oracle for two constant-weight Skin islands."""
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
assert launch['motionHistory'] and launch['islandHistory'] and launch['subdivisions']==24 and launch['maximumBonesPerVertex']==2
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
contracts=[r for r in rows if r['event']=='island_contract'];assert len(contracts)==1
contract=contracts[0];weight_pairs=[[[.25,.75],[.625,.375]],[[-.25,1.25],[1.5,-.5]]]
assert contract['vertices']==650 and contract['islandVertices']==325 and contract['islands']==2 and contract['subdivisions']==24
assert contract['separationPixels']==288 and not contract['skinCategoryIgnoresMotion']
assert contract['weights']==weight_pairs and contract['scale']==.55 and contract['offsetY']==[.8,-.8]
retirements=[r for r in rows if r['event']=='island_meshes_retired']
assert [(r['signedProfile'],r['count']) for r in retirements]==[(0,3),(1,3)]
completion=[r for r in rows if r['event']=='deviceCompletion'];assert len(completion)==1 and completion[0]['polls']>0
assert completion[0]['milliseconds']<=10000
configuration={r['key']:r['value'] for r in rows if r['event']=='config'}
for key,value in {'rtx.upscalerType':'0','rtx.resolutionScale':'1','rtx.forceCameraJitter':'False',
                  'rtx.postfx.enableMotionBlur':'False','rtx.enableRayReconstruction':'False'}.items():assert configuration[key]==value
phases=[r for r in rows if r['event']=='island_phase']
assert [(r['signedProfile'],r['phase']) for r in phases]==[(s,p) for s in range(2) for p in range(5)]
names=[f'island-s{s}-p{p}.bmp' for s in range(2) for p in range(5)]
assert [r['file'] for r in phases]==names and [r['file'] for r in rows if r['event']=='capture']==names
results=[];distinct=[]
for phase in phases:
    image=Image.open(run/phase['file']).convert('RGB');assert image.size==(960,540)
    phase_results=[]
    expected_pose=(0,0,0) if phase['phase'] in (0,1,4) else (0,.00125,0) if phase['phase']==2 else (.002,.000625,.00046875)
    assert all(abs(abs(phase[key])-want)<1e-8 for key,want in zip(('angle','dx','dy'),expected_pose))
    for island,y0 in enumerate((198,352)):
        a,b=weight_pairs[phase['signedProfile']][island];k=a-b
        c=math.cos(phase['angle']);s=k*math.sin(phase['angle']);det=c*c+s*s
        tx=k*phase['dx'];ty=k*phase['dy']
        left=[];right=[];expected=[]
        for y in range(y0,y0+16):
            for x in range(328,344):
                left.append(image.getpixel((x,y)));right.append(image.getpixel((x+288,y)))
                qx=(x+.5-336)/96;qy=(289.2-y-.5)/96
                # Recover authored local point from the current affine map,
                # then project it through the independently known previous map.
                ux=(c*(qx-tx)+s*(qy-ty))/det;uy=(-s*(qx-tx)+c*(qy-ty))/det
                previous_x=c*ux+s*uy-tx;previous_y=-s*ux+c*uy-ty
                expected.append([255*min(1,abs((qx-previous_x)*96)),255*min(1,abs((qy-previous_y)*96)),0])
        pair_errors=[abs(a[ch]-b[ch]) for a,b in zip(left,right) for ch in range(3)]
        checks=dict(settled=phase['frames']>=90,pairedMaximum=max(pair_errors)<=4,pairedMean=sum(pair_errors)/len(pair_errors)<=2)
        summary={}
        if phase['phase']==0:checks['visible']=all(min(p)>=200 for pixels in (left,right) for p in pixels)
        elif phase['phase'] in (1,4):checks['returnedToZero']=all(max(p)<=3 for pixels in (left,right) for p in pixels)
        else:
            expected_ranges=[max(v[ch] for v in expected)-min(v[ch] for v in expected) for ch in range(3)]
            residuals=[];ranges=[]
            for pixels in (left,right):
                errors=[abs(actual[ch]-want[ch]) for actual,want in zip(pixels,expected) for ch in range(3)]
                residuals.append(dict(maximum=max(errors),mean=sum(errors)/len(errors)))
                ranges.append([max(v[ch] for v in pixels)-min(v[ch] for v in pixels) for ch in range(3)])
            checks['analyticMotion']=all(e['maximum']<=4 and e['mean']<=1.5 for e in residuals)
            checks['unusedChannelZero']=all(p[2]<=3 for pixels in (left,right) for p in pixels)
            if phase['phase']==3:
                checks['spatialGradient']=expected_ranges[0]>3 and all(r[0]>=2 for r in ranges)
            summary=dict(analyticErrors=residuals,actualRanges=ranges,expectedRanges=expected_ranges)
        item=dict(**phase,island=island,checks=checks,passed=all(checks.values()),pairedMaximum=max(pair_errors),
                  referenceMean=[sum(v[ch] for v in left)/len(left) for ch in range(3)],
                  skinMean=[sum(v[ch] for v in right)/len(right) for ch in range(3)],**summary)
        results.append(item);phase_results.append(item)
    if phase['phase']==2:
        difference=abs(phase_results[0]['skinMean'][0]-phase_results[1]['skinMean'][0])
        distinct.append(dict(profile=phase['signedProfile'],redDifference=difference,passed=difference>=8))
files=['launch.json','exit.json','cleanup.json','build.json','fixture.jsonl',*names]
passed=all(r['passed'] for r in results) and all(r['passed'] for r in distinct)
report=dict(status='PASS' if passed else 'FAIL',run=str(run),phases=results,distinctIslandMotion=distinct,
    contract=contract,complete=rows[-1],deviceCompletion=completion[0],execution=execution,cleanup=cleanup,
    rendererSha256=launch['rendererSha256'],hashes={p:digest(run/p) for p in files},
    scope='One immutable650-vertex Skin mesh per positive/signed B2 profile, two325-vertex islands with different constant weights. Two independent rigid affine reference instances; per-pixel RGB8 analytic motion, translation/rotation and return to rest. No continuously varying vertex weights, camera motion, DLSS/TAA or full-game history qualification.')
output.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(dict(status=report['status'],phases=len(phases),islandChecks=len(results),
    maximumPairedDifference=max(r['pairedMaximum'] for r in results),distinctIslandMotion=distinct,
    failed=[r for r in results if not r['passed']]),indent=2))
assert passed
