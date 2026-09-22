"""Check rendered triangle attributes against independent sparse source samples."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import os
import re
import shutil
import sys
import traceback

parser=argparse.ArgumentParser()
parser.add_argument('--run-directory',type=Path,required=True)
parser.add_argument('--usd-directory',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
options=parser.parse_args();run=options.run_directory.resolve();output=options.output.resolve()
assert not output.exists(), 'Preserve existing result'
sources=output.with_name(output.stem+'-sources');sources.mkdir()
for p in (Path(__file__),Path(__file__).with_name('Test-UsdExporter.ps1')):shutil.copy2(p,sources/p.name)
usd=options.usd_directory.resolve()
handles=[os.add_dll_directory(str(usd/p)) for p in ('lib','bin')];sys.path.insert(0,str(usd/'lib/python'))
from pxr import Usd,UsdGeom,UsdShade
report=dict(status='FAIL',name=run.name,checks=[],scenes=[],readerSources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources.iterdir()})
def check(name,ok,**details):report['checks'].append(dict(name=name,passed=bool(ok),**details))
def value(a):
    if a is None or isinstance(a,(bool,int,float,str)):return a
    return [value(v) for v in a]
def flat(a):
    if isinstance(a,(bool,int,float)):return [a]
    return [x for v in a for x in flat(v)]
def near(name,a,b):
    av=flat(a) if a is not None else [];bv=flat(b)
    delta=max([abs(x-y) for x,y in zip(av,bv)]+[0]) if len(av)==len(bv) and all(math.isfinite(x) for x in av+bv) else 1e300
    check(name,delta<=2e-6,maxError=delta)
try:
    execution=json.loads((run/'execution.json').read_text(encoding='utf-8-sig'))
    check('native clean CPU completion',execution['exitCode']==0 and execution['signaled'] and not any(execution[k] for k in ('forced','memoryLimited','timedOut','remaining')))
    check('sixteen completed exports',(run/'stdout.log').read_text(encoding='utf-8').count('EXPORTED ')==16)
    for case in ('stable','changing','growing','shrinking','constant-stable','constant-changing','growing-faces','shrinking-faces'):
      for reduce in (False,True):
        changing=case in ('changing','constant-changing')
        variable_vertices=case in ('growing','shrinking')
        constant_color=case.startswith('constant-')
        name=case+('-reduced' if reduce else '-full');directory=run/'captures'/name
        stage=Usd.Stage.Open(str(directory/'own_dynamic.usda'));scene=dict(name=name,samples=[]);report['scenes'].append(scene)
        check(name+' stage interval',bool(stage) and stage.GetStartTimeCode()==0 and stage.GetEndTimeCode()==3 and stage.GetTimeCodesPerSecond()==24)
        meshes=[p for p in stage.Traverse() if p.IsA(UsdGeom.Mesh)]
        check(name+' one prototype and one instance',len(meshes)==2)
        for prim in meshes:
            mesh=UsdGeom.Mesh(prim);pv=UsdGeom.PrimvarsAPI(prim);label=name+str(prim.GetPath())
            check(label+' synchronized topology samples',mesh.GetFaceVertexIndicesAttr().GetTimeSamples()==mesh.GetFaceVertexCountsAttr().GetTimeSamples()==[0.,2.])
            for t in (0.,.5,1.,1.5,2.,3.):
                prefix=label+' t'+str(t);pt=t;ct=min(t,2)
                if variable_vertices:pt=ct=0 if t<2 else 2
                ids=([0,2,4] if t<2 else [1,3,5]) if changing else ([1,3,5] if t<2 else [5,3,1])
                if variable_vertices:ids=[0,1,2] if t<2 else [2,1,0]
                if (case=='growing-faces' and t>=2) or (case=='shrinking-faces' and t<2):ids+=ids[::-1]
                want_points=[[j*2+pt*.125,j%2+pt*.25,5+(j%3)*.125] for j in ids]
                want_uv=[[j*.125,ct*.25] for j in ids]
                want_color=[[(j+1)*.1,ct*.05,.25] for j in ids]
                want_opacity=[.2+j*.1+ct*.05 for j in ids]
                if constant_color:
                    want_color=[[.25,.5+ct*.05,.75]]*len(ids)
                    want_opacity=[.625+ct*.05]*len(ids)
                points=value(mesh.GetPointsAttr().Get(t));indices=value(mesh.GetFaceVertexIndicesAttr().Get(t))
                uv=value(pv.GetPrimvar('st').Get(t));color=value(pv.GetPrimvar('displayColor').Get(t));opacity=value(pv.GetPrimvar('displayOpacity').Get(t))
                normals=value(mesh.GetNormalsAttr().Get(t))
                row=dict(path=str(prim.GetPath()),time=t,points=points,indices=indices,uv=uv,color=color,opacity=opacity)
                scene['samples'].append(row)
                valid=indices is not None and len(indices)==len(ids) and all(0<=i<len(points) for i in indices)
                check(prefix+' valid triangle topology',valid and value(mesh.GetFaceVertexCountsAttr().Get(t))==[3]*(len(ids)//3))
                check(prefix+' per-vertex attribute counts',all(a is not None and len(a)==len(points) for a in (normals,uv)),
                    points=len(points),normals=len(normals),uv=len(uv),color=len(color),opacity=len(opacity))
                color_count=1 if constant_color else len(points)
                color_interpolation='constant' if constant_color else 'vertex'
                check(prefix+' color cardinality and interpolation',len(color)==len(opacity)==color_count and
                    pv.GetPrimvar('displayColor').GetInterpolation()==pv.GetPrimvar('displayOpacity').GetInterpolation()==color_interpolation)
                expected_count=6
                if variable_vertices:expected_count=3 if ((t<2) != (case=='shrinking')) else 6
                if reduce and not changing and not variable_vertices:expected_count=3
                check(prefix+' exported vertex count',len(points)==expected_count,actual=len(points),expected=expected_count)
                if valid:
                    near(prefix+' triangle positions',[points[i] for i in indices],want_points)
                    near(prefix+' triangle normals',[normals[i] for i in indices],[[0,0,-1]]*len(ids))
                    near(prefix+' triangle UV',[uv[i] for i in indices],want_uv)
                    near(prefix+' triangle color',[color[0 if constant_color else i] for i in indices],want_color)
                    near(prefix+' triangle opacity',[opacity[0 if constant_color else i] for i in indices],want_opacity)
                visible=UsdGeom.Imageable(prim).ComputeVisibility(t)=='inherited'
                check(prefix+' visibility',visible==('/instances/' in str(prim.GetPath())))
        scene['outputHashes']={p.relative_to(directory).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.rglob('*') if p.is_file()}
    report['status']='PASS' if all(c['passed'] for c in report['checks']) else 'FAIL'
except Exception:report['error']=traceback.format_exc()
report['scope']='Own CPU exporter sparse rigid geometry, constant/per-vertex color, changing vertex sets, attribute lengths and triangle counts. Exact and intermediate time samples; triangle comparison independent of compaction strategy. No GPU/game qualification.'
with output.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
failures=[c for c in report['checks'] if not c['passed']]
print(json.dumps(dict(status=report['status'],checks=len(report['checks']),failed=len(failures),firstFailures=failures[:12],error=report.get('error'))))
sys.exit(0 if report['status']=='PASS' else 1)
