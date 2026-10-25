"""Own CPU oracle for pinned USD, independent of renderer capture output."""
from pathlib import Path
import hashlib
import json
import os
import sys
import traceback

import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--usd-directory',type=Path,required=True,help='USD distribution root containing lib/python, lib and bin')
parser.add_argument('--output',type=Path,required=True,help='New JSON evidence file; existing output is rejected')
options=parser.parse_args()
output=options.output.resolve()
if output.exists():raise FileExistsError('Preserve previous result: '+str(output))
output.parent.mkdir(parents=True,exist_ok=True)
usd=options.usd_directory.resolve()
handles=[os.add_dll_directory(str(usd/n)) for n in ('lib','bin')]
sys.path.insert(0,str(usd/'lib/python'))
from pxr import Usd, UsdGeom, UsdSkel, Gf, Vt

result=dict(status='FAIL',scope='Own CPU-only USD library weight semantics; no renderer, IPC, captured game data or GPU qualification.',cases=[],checks=[])
def check(name,value):result['checks'].append(dict(name=name,passed=bool(value)))
def vectors(points):return [[float(x) for x in p] for p in points]
def error(a,b):return max(abs(x-y) for p,q in zip(a,b) for x,y in zip(p,q))
try:
    names=[name for name in dir(UsdSkel) if 'SkinPoints' in name]
    result['availableSkinPointFunctions']=names
    name=next(n for n in ('SkinPoints','SkinPointsLBS') if n in names)
    function=getattr(UsdSkel,name);result['function']=name
    translations=[[.25,.125,0],[-.125,-.25,0],[.125,-.0625,.125],[-.0625,.25,-.125]]
    matrices=[]
    for t in translations:
        matrix=Gf.Matrix4d(1);matrix.SetTranslate(Gf.Vec3d(*t));matrices.append(matrix)
    palette=Vt.Matrix4dArray(matrices)
    weights_by_count=[
        [.5,1,1.25,.75],
        [-.25,1.25,.75,.25,1.5,-.5,0,1],
        [.25,.5,.25,-.25,.75,.5,.125,.25,.25,0,0,1.25],
        [.125,.25,.375,.25,-.5,.25,.75,.5,.5,.25,.25,.25,0,0,0,.75],
    ]
    for n,weights in enumerate(weights_by_count,1):
        indices=[(i+k)%4 for i in range(4) for k in range(n)]
        rest=[[1.25+(.5 if i%2 else -.5),(n-2.5)*.75+(-.25 if i//2 else .25),5] for i in range(4)]
        expected=[[sum(weights[i*n+k]*(rest[i][axis]+translations[indices[i*n+k]][axis]) for k in range(n)) for axis in range(3)] for i in range(4)]
        points=Vt.Vec3fArray([Gf.Vec3f(*p) for p in rest]);wi=Vt.FloatArray(weights);ji=Vt.IntArray(indices)
        ok=function(Gf.Matrix4d(1),palette,ji,wi,n,points)
        actual=vectors(points)
        check(str(n)+' influences: direct LBS executed',ok)
        check(str(n)+' influences: direct LBS preserves explicit weighted sum',error(actual,expected)<=2e-6)
        check(str(n)+' influences: input weights and joints preserved',list(wi)==weights and list(ji)==indices)
        # Build only anonymous USD objects to exercise the same SkinningQuery
        # entry point as capture readback. Nothing is saved over historical data.
        stage=Usd.Stage.CreateInMemory();skelroot=UsdSkel.Root.Define(stage,'/Root')
        skeleton=UsdSkel.Skeleton.Define(stage,'/Root/Skeleton')
        joints=Vt.TokenArray(['root','root/joint1','root/joint2','root/joint3'])
        skeleton.CreateJointsAttr(joints)
        skeleton.CreateBindTransformsAttr(Vt.Matrix4dArray([Gf.Matrix4d(1)]*4))
        skeleton.CreateRestTransformsAttr(Vt.Matrix4dArray([Gf.Matrix4d(1)]*4))
        mesh=UsdGeom.Mesh.Define(stage,'/Root/Mesh');mesh.CreatePointsAttr(Vt.Vec3fArray([Gf.Vec3f(*p) for p in rest]))
        binding=UsdSkel.BindingAPI.Apply(mesh.GetPrim())
        binding.CreateSkeletonRel().SetTargets([skeleton.GetPath()])
        binding.CreateGeomBindTransformAttr(Gf.Matrix4d(1))
        binding.CreateJointIndicesPrimvar(False,n).Set(ji)
        binding.CreateJointWeightsPrimvar(False,n).Set(wi)
        cache=UsdSkel.Cache();check(str(n)+' influences: anonymous root populated',cache.Populate(skelroot,Usd.PrimDefaultPredicate))
        query=cache.GetSkinningQuery(mesh.GetPrim());check(str(n)+' influences: skin query valid',bool(query))
        query_points=Vt.Vec3fArray([Gf.Vec3f(*p) for p in rest])
        query_ok=query.ComputeSkinnedPoints(palette,query_points,Usd.TimeCode.Default())
        query_actual=vectors(query_points)
        check(str(n)+' influences: SkinningQuery executed',query_ok)
        check(str(n)+' influences: SkinningQuery preserves explicit weighted sum',error(query_actual,expected)<=2e-6)
        result['cases'].append(dict(influences=n,weights=weights,indices=indices,rest=rest,expected=expected,
            direct=actual,query=query_actual,directError=error(actual,expected),queryError=error(query_actual,expected)))
    result['status']='PASS' if all(c['passed'] for c in result['checks']) else 'FAIL'
except Exception:
    result['error']=traceback.format_exc()
result['sourceSha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
result['usdVersion']=list(Usd.GetVersion())
with output.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
print(json.dumps(dict(status=result['status'],functions=result.get('availableSkinPointFunctions'),checks=len(result['checks']),
    failed=[c['name'] for c in result['checks'] if not c['passed']],error=result.get('error'))))
sys.exit(0 if result['status']=='PASS' else 1)
