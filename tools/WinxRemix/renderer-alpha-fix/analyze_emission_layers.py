import argparse
import hashlib
import json
import re
from pathlib import Path
from PIL import Image

parser=argparse.ArgumentParser()
parser.add_argument('name')
parser.add_argument('output',type=Path,nargs='?')
parser.add_argument('--expect',choices=['measure','stock-failure','pass'],default='pass')
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
assert re.fullmatch(r'[a-zA-Z0-9_-]+',args.name),'Invalid run name'
run=root/'local-data/rtx-remix/material-fixtures'/args.name
output=(args.output or run/'analysis.json').resolve();output.relative_to(root);assert not output.exists()
records=[json.loads(line) for line in (run/'fixture.jsonl').read_text().splitlines()]
assert records[-1]['event']=='complete' and not any(r['event']=='error' for r in records)
pairs=[r for r in records if r['event']=='pair'];assert len(pairs)==4
captures=[r for r in records if r['event']=='capture'];assert len(captures)==2
assert [r['file'] for r in captures]==['forward.bmp','reverse.bmp']
assert [(r['index'],r['sourcePanel'],r['referencePanel'],r['layers']) for r in pairs]==[(0,0,1,1),(1,2,3,2),(2,4,5,3),(3,6,7,3)]
assert records[-1]['frames']>=360 and records[-1]['meshesRetired']==13 and records[-1]['materialsRetired']==13
completion=[r for r in records if r['event']=='deviceCompletion']
assert len(completion)==1 and completion[0]['polls']>0 and completion[0]['milliseconds']<=10000
execution=json.loads((run/'execution.json').read_text(encoding='utf-8-sig'))
lifecyclePassed=(execution['exitCode']==0 and not execution['memoryLimited'] and not execution['timedOut']
    and not execution['remaining'] and bool(execution['servers'])
    and all(s['exitCode']==0 and s['exitedWithinWait'] for s in execution['servers']))
assert lifecyclePassed,'The complete owned operation must finish cleanly before comparing images'
observations=[]
for capture in captures:
    picture=Image.open(run/capture['file']).convert('RGB');assert picture.size==(960,540)
    values=[]
    for panel in range(8):
        x=192+192*(panel%4);y=160 if panel<4 else 380
        samples=[picture.getpixel((xx,yy)) for yy in range(y-10,y+11) for xx in range(x-10,x+11)]
        values.append([sum(v[k] for v in samples)/len(samples) for k in range(3)])
    comparison=[]
    for pair in pairs:
        actual=values[pair['sourcePanel']];expected=values[pair['referencePanel']]
        error=max(abs(a-b) for a,b in zip(actual,expected))
        referenceVisible=max(expected)>10
        displayReference=[255*max(0,e)**(1/2.2) for e in pair['expectedEmission']]
        referenceEncodingError=max(abs(a-b) for a,b in zip(expected,displayReference))
        comparison.append(dict(pair=pair['index'],source=actual,reference=expected,maxMeanError=error,referenceVisible=referenceVisible,
                               expectedReferenceDisplay=displayReference,referenceEncodingError=referenceEncodingError,
                               passed=error<=2 and referenceVisible and referenceEncodingError<=2))
    observations.append(dict(file=capture['file'],panels=values,comparison=comparison))
orderErrors=[max(abs(a-b) for a,b in zip(x,y)) for x,y in zip(observations[0]['panels'],observations[1]['panels'])]
referenceSpread=max(abs(a-b) for o in observations for left in o['comparison'] for right in o['comparison'] for a,b in zip(left['reference'],right['reference']))
passed=all(c['passed'] for o in observations for c in o['comparison']) and max(orderErrors)<=2 and referenceSpread>10
result=dict(run=args.name,pairs=pairs,views=observations,drawOrderMaxMeanErrors=orderErrors,passed=passed,
            tolerance=2,referenceSpread=referenceSpread,scope='Same-frame analytic emission reference panels; 21x21 interior ROIs in 8-bit RGB. No game/fog/volumetric qualification.',
            complete=records[-1],deviceCompletion=completion[0],execution=execution,lifecyclePassed=lifecyclePassed,
            sourceHashes={name:hashlib.sha256((run/name).read_bytes()).hexdigest().upper() for name in
                ('test_emission_layers.cpp','fixture_helpers.h','winx_surface_instance.h','winx_surface_material.h','winx_material_channels.h','winx_material_channel_assets.h','rtx.conf','dxvk.conf','fixture.jsonl','forward.bmp','reverse.bmp')},
            rendererSha256=hashlib.sha256((run/'.trex/d3d9.dll').read_bytes()).hexdigest().upper())
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(dict(run=args.name,passed=passed,pairErrors=[[c['maxMeanError'] for c in o['comparison']] for o in observations],
    referenceEncodingErrors=[[c['referenceEncodingError'] for c in o['comparison']] for o in observations],
    drawOrderMaxMeanErrors=orderErrors,execution=result['execution']),indent=2))
if args.expect=='pass':
    assert passed,'Emission does not match the reference'
elif args.expect=='stock-failure':
    assert not passed and all(o['comparison'][0]['maxMeanError']>20 for o in observations),'Expected stock loss was not reproduced'
