"""Verify saved native live results after owned-process cleanup; launches nothing.

The early negative runners read a trace handle before Windows had released it.
Preserve those failed wrapper reports and independently verify the native exit,
fault-injection ownership, exact log marker and final saved trace.
"""
from pathlib import Path
import hashlib
import json
import re

ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/'local-data/rtx-remix/direct-camera-live'
output=BASE/'closed-evidence-v1.json'
if output.exists():raise RuntimeError('Historical evidence output already exists')
entries=[]
for name in ['positive-v2','fault-v1','fault-v2']:
    directory=BASE/name
    result=json.loads((directory/'result.json').read_text())
    journal=[json.loads(line) for line in (directory/'helper.jsonl').read_text().splitlines()]
    assert not result['jobAfterCleanup'] and result['jobAssignedBeforeResume']
    assert result['elapsedSeconds']<30 and not result.get('timeout')
    assert Path(result['verifiedImage']).resolve()==(directory/'live_camera.exe').resolve()
    for entry in result['preparedFiles']:
        assert hashlib.sha256((directory/entry['path']).read_bytes()).hexdigest().upper()==entry['sha256']
    cameras=[r for r in journal if r['event']=='camera']
    assert [r['result'] for r in cameras[:5]]==[7,3,3,3,7]
    row={'run':name,'nativeVerified':True,'originalWrapperPassed':result['passed'],'originalWrapperError':result.get('error'),'exitCode':result['exitCode'],'elapsedSeconds':result['elapsedSeconds'],'evidence':[]}
    if name=='positive-v2':
        assert result['exitCode']==0 and result['passed']
        assert len(cameras)==25 and all(r['result']==0 for r in cameras[5:])
        assert [r['frame'] for r in cameras[5:]]==list(range(20))
        assert len({tuple(r['view']) for r in cameras[5:]})==20
        assert [r['frame'] for r in journal if r['event']=='present']==list(range(20))
        assert journal[-1]['event']=='complete' and journal[-1]['frames']==20
        row['complete']=journal[-1]
    else:
        assert result['exitCode']==0xE052CA01
        assert journal[-1]['event']=='fault-camera-call'
        assert 'Permission denied' in result.get('error','')
        suspension=result['suspendedOwnedServer']
        assert Path(suspension['path']).resolve()==(directory/'.trex/NvRemixBridge.exe').resolve()
        assert suspension['threads'] and suspension['pid']!=result['pid']
        log=(directory/'rtx-remix/logs/bridge32.log').read_text()
        marker=re.search(r'Fatal transport failure; UID=(\d+) result=(\d+); terminating client, no recovery\.',log)
        assert marker and int(marker[2])!=0
        row['fatalUid']=int(marker[1]);row['transportResult']=int(marker[2])
        row['scope']='Terminal response-loss proof after CreateDevice; renderer server suspended by verified owned thread handles; source helper retained in run directory'
    for path in [directory/'result.json',directory/'launch.json',directory/'helper.jsonl',directory/'prepared.json',directory/'live_camera.cpp',directory/'live_camera.exe',*sorted((directory/'rtx-remix/logs').glob('*.log'))]:
        row['evidence'].append({'path':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(path.read_bytes()).hexdigest().upper(),'bytes':path.stat().st_size})
    entries.append(row)
report={'status':'Saved native positive and two terminal-negative runs verified after cleanup; original failed wrapper reports preserved','launchesPerformed':0,'cases':entries,'limitations':['No renderer matrix readback or pixel-motion verification','Latest reproducer moves fault gate before device and adds bounded trace-read retry; that variant did not reach fault injection in fault-v4 because cold server startup exceeded timeout']}
output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'output':str(output),'nativeVerified':len(entries),'launchesPerformed':0},indent=2))
