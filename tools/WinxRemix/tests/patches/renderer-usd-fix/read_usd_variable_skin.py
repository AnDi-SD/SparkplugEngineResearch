"""Own variable Skin readback: uniform scenes grow6->8; nonuniform shrink8->6."""
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
parser.add_argument('--default-joints',action='store_true')
options=parser.parse_args();run=options.run_directory.resolve();output=options.output.resolve()
assert not output.exists()
sources=output.with_name(output.stem+'-sources');sources.mkdir()
for p in (Path(__file__),Path(__file__).with_name('Test-UsdExporter.ps1')):shutil.copy2(p,sources/p.name)
usd=options.usd_directory.resolve();handles=[os.add_dll_directory(str(usd/p)) for p in ('lib','bin')]
sys.path.insert(0,str(usd/'lib/python'))
from pxr import Usd,UsdGeom,UsdSkel,Gf,Vt

report=dict(status='FAIL',checks=[],scenes=[],jointIndices='default' if options.default_joints else 'explicit',
    readerSources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources.iterdir()})
def check(name,ok,**details):report['checks'].append(dict(name=name,passed=bool(ok),**details))
def value(a):
    if a is None or isinstance(a,(int,float,str,bool)):return a
    return [value(v) for v in a]
def flat(a):
    if isinstance(a,(int,float)):return [a]
    return [x for v in a for x in flat(v)]
def near(name,a,b):
    av=flat(a) if a is not None else [];bv=flat(b)
    delta=max([abs(x-y) for x,y in zip(av,bv)]+[0]) if len(av)==len(bv) and all(math.isfinite(x) for x in av+bv) else 1e300
    check(name,delta<=2e-6,maxError=delta)
    return delta
def pose(t,j,nonuniform):
    a=math.radians(45*j*(1 if t==1 else -1)) if t else 0
    sx=1+(j+1)/8 if t else 1
    sy=(1+j/4 if nonuniform else sx) if t else 1
    sz=(1.5 if nonuniform else sx) if t else 1
    return [[sx*math.cos(a),sx*math.sin(a),0,0],[-sy*math.sin(a),sy*math.cos(a),0,0],
            [0,0,sz,0],[t*(j+1)/8,-t*j/16,t*(j+1)/32,1]]
def transform(p,m):return [sum(p[k]*m[k][a] for k in range(3))+m[3][a] for a in range(3)]
weights=[ [.5,1,1.25,.75],[-.25,1.25,.75,.25,1.5,-.5,0,1],
          [.25,.5,.25,-.25,.75,.5,.125,.25,.25,0,0,1.25],
          [.125,.25,.375,.25,-.5,.25,.75,.5,.5,.25,.25,.25,0,0,0,.75] ]
try:
    execution=json.loads((run/'execution.json').read_text(encoding='utf-8-sig'))
    check('clean native completion',execution['exitCode']==0 and execution['signaled'] and not any(execution[k] for k in ('forced','memoryLimited','timedOut','remaining')))
    check('four completed exports',(run/'stdout.log').read_text(encoding='utf-8').count('EXPORTED ')==4)
    for nonuniform in (False,True):
      for reduced in (False,True):
        name=('nonuniform' if nonuniform else 'uniform')+('-reduced' if reduced else '-full')
        directory=run/'captures'/name;stage=Usd.Stage.Open(str(directory/'own_offline.usda'))
        scene=dict(name=name,direction='shrinking' if nonuniform else 'growing',samples=[]);report['scenes'].append(scene)
        check(name+' stage timing',bool(stage) and stage.GetStartTimeCode()==0 and stage.GetEndTimeCode()==2 and stage.GetTimeCodesPerSecond()==24)
        cache=UsdSkel.Cache()
        roots=[p for p in stage.Traverse() if p.IsA(UsdSkel.Root)]
        check(name+' twelve skeleton roots',len(roots)==12)
        for p in roots:check(name+str(p.GetPath())+' cache populated',cache.Populate(UsdSkel.Root(p),Usd.PrimDefaultPredicate))
        meshes=[p for p in stage.Traverse() if p.IsA(UsdGeom.Mesh)];check(name+' twelve meshes',len(meshes)==12)
        for prim in meshes:
            path=str(prim.GetPath());label=name+path;n=int(re.search(r'(?:mesh_|inst_)case([1-4])',path)[1]);c=n-1
            mesh=UsdGeom.Mesh(prim);pv=UsdGeom.PrimvarsAPI(prim);binding=UsdSkel.BindingAPI(prim)
            js=binding.GetJointIndicesPrimvar();ws=binding.GetJointWeightsPrimvar()
            check(label+' synchronized vertex and weight samples',mesh.GetPointsAttr().GetTimeSamples()==ws.GetAttr().GetTimeSamples()==js.GetAttr().GetTimeSamples()==[0.,2.])
            check(label+' influence width',ws.GetElementSize()==js.GetElementSize()==n and ws.GetInterpolation()==js.GetInterpolation()=='vertex')
            skeleton=binding.GetInheritedSkeleton();query=cache.GetSkelQuery(skeleton);skin=cache.GetSkinningQuery(prim)
            check(label+' palette queries',bool(query) and bool(skin) and list(query.GetJointOrder())==['joint0','joint1','joint2','joint3'])
            prototype='/meshes/' in path;late='_late/' in path
            for t in range(3):
                prefix=label+' t'+str(t);pt=0 if t<2 else 2;factor=1+pt*.125
                count=8 if ((t<2)==nonuniform) else 6
                slots=[None,1,3,None,0,2]+([None,None] if count==8 else [])
                logical=[[1.25+(-.5 if i%2==0 else .5)+pt*.125,(c-1.5)*.75+(.25 if i<2 else -.25)+pt*.25,5.] for i in range(4)]
                want_points=[logical[i] if i is not None else [99.,99.,99.] for i in slots]
                want_weights=[x*factor for i in slots for x in (weights[c][i*n:(i+1)*n] if i is not None else [0.]*n)]
                want_joints=[(k if options.default_joints else ((i+k)%4 if i is not None else 0)) for i in slots for k in range(n)]
                points=value(mesh.GetPointsAttr().Get(t))
                near(prefix+' held or endpoint positions',points,want_points)
                near(prefix+' explicit signed weights',value(ws.Get(t)),want_weights)
                check(prefix+' joint indices and cardinality',value(js.Get(t))==want_joints)
                check(prefix+' preserved triangle topology',value(mesh.GetFaceVertexCountsAttr().Get(t))==[3,3] and value(mesh.GetFaceVertexIndicesAttr().Get(t))==[4,1,5,5,1,2])
                near(prefix+' normals',value(mesh.GetNormalsAttr().Get(t)),[[0,0,-1]]*count)
                near(prefix+' UV',value(pv.GetPrimvar('st').Get(t)),[[i%2,i//2] if i is not None else [0,0] for i in slots])
                near(prefix+' color',value(pv.GetPrimvar('displayColor').Get(t)),[[.25+pt*.05,.5,.75]]*count)
                near(prefix+' opacity',value(pv.GetPrimvar('displayOpacity').Get(t)),[.625+pt*.025]*count)
                visible=not prototype and (not late or t==1)
                check(prefix+' visibility',(UsdGeom.Imageable(prim).ComputeVisibility(t)=='inherited')==visible)
                if not visible:continue
                actual=query.ComputeSkinningTransforms(t);palette=[pose(t,j,nonuniform) for j in range(4)]
                matrix_error=near(prefix+' reconstructed palette',value(actual),palette)
                deformed=Vt.Vec3fArray([Gf.Vec3f(*p) for p in points])
                check(prefix+' USD Skin operation',skin.ComputeSkinnedPoints(actual,deformed,t))
                want=[]
                for i in slots:
                    if i is None:want.append([0.,0.,0.]);continue
                    transformed=[transform(logical[i],palette[k if options.default_joints else (i+k)%4]) for k in range(n)]
                    want.append([sum(weights[c][i*n+k]*factor*transformed[k][a] for k in range(n)) for a in range(3)])
                point_error=near(prefix+' deformed vertices',value(deformed),want)
                scene['samples'].append(dict(path=path,time=t,vertices=count,matrixError=matrix_error,pointError=point_error))
        scene['outputHashes']={p.relative_to(directory).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.rglob('*') if p.is_file()}
    report['status']='PASS' if all(c['passed'] for c in report['checks']) else 'FAIL'
except Exception:report['error']=traceback.format_exc()
report['scope']='Own CPU variable Skin arrays6/8, source samples0/2, rendered frames0/1/2. Uniform palette grows, nonuniform shrinks;1..4 signed influences and explicit/default joints, full/reduced request. No GPU or interpolation between bone-pose samples.'
with output.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
failures=[c for c in report['checks'] if not c['passed']]
print(json.dumps(dict(status=report['status'],checks=len(report['checks']),failed=len(failures),firstFailures=failures[:8],error=report.get('error'),
    maxPoint=max([s['pointError'] for scene in report['scenes'] for s in scene['samples']]+[0]))))
sys.exit(0 if report['status']=='PASS' else 1)
