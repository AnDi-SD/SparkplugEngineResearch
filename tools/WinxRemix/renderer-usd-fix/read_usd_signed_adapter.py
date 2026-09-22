"""Read actual exporter output. Expected transforms use independent scalar math."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import os
import re
import shutil
import struct
import sys
import traceback

parser=argparse.ArgumentParser()
parser.add_argument('--run-directory',type=Path,required=True)
parser.add_argument('--usd-directory',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--topology',choices=['star','siblings'],default='siblings')
parser.add_argument('--single-frame',action='store_true')
options=parser.parse_args()
run=options.run_directory.resolve()
output=options.output.resolve()
assert not output.exists(), 'Preserve existing result'
sources=output.with_name(output.stem+'-sources')
sources.mkdir()
for source_file in (Path(__file__),Path(__file__).with_name('Test-UsdExporter.ps1')):
    shutil.copy2(source_file,sources/source_file.name)
usd=options.usd_directory.resolve()
handles=[os.add_dll_directory(str(usd/p)) for p in ('lib','bin')]
sys.path.insert(0,str(usd/'lib/python'))
from pxr import Usd,UsdGeom,UsdSkel,UsdShade,Gf,Vt,Sdf

report=dict(status='FAIL',name=run.name,usdVersion=list(Usd.GetVersion()),checks=[],scenes=[])
report['readerSources']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources.iterdir()}
def check(name,value,**detail):report['checks'].append(dict(name=name,passed=bool(value),**detail))
def flatten(a):
    if isinstance(a,(int,float)):return [a]
    return [x for v in a for x in flatten(v)]
def error(a,b):
    if a is None or b is None:return 1e300
    av=flatten(a);bv=flatten(b)
    if not all(math.isfinite(x) for x in av+bv):return 1e300
    return max([abs(x-y) for x,y in zip(av,bv)]+[0]) if len(av)==len(bv) else 1e300
def near(name,a,b,tol=2e-6):
    delta=error(a,b);check(name,delta<=tol,maxError=delta,tolerance=tol)
def value(a):
    if a is None or isinstance(a,(int,float,str,bool)):return a
    if isinstance(a,Sdf.AssetPath):return dict(path=a.path,resolved=a.resolvedPath)
    return [value(v) for v in a]
def pose(t,j,nonuniform):
    a=math.radians(45*j*(1 if t==1 else -1)) if t else 0
    sx=1+(j+1)/8 if t else 1
    sy=(1+j/4 if nonuniform else sx) if t else 1
    sz=(1.5 if nonuniform else sx) if t else 1
    return [[sx*math.cos(a),sx*math.sin(a),0,0],[-sy*math.sin(a),sy*math.cos(a),0,0],
            [0,0,sz,0],[t*(j+1)/8,-t*j/16,t*(j+1)/32,1]]
def adapter_palette(t,nonuniform):
    positive=[pose(t,j,nonuniform) for j in range(4)]
    negative=[[[(-v if c<3 else v) for c,v in enumerate(row)] for row in m] for m in positive]
    return positive+negative+[[[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,1]]]

def transform(p,m):return [sum(p[k]*m[k][a] for k in range(3))+m[3][a] for a in range(3)]
weights=[ [.5,1,1.25,.75],[-.25,1.25,.75,.25,1.5,-.5,0,1],
          [.25,.5,.25,-.25,.75,.5,.125,.25,.25,0,0,1.25],
          [.125,.25,.375,.25,-.5,.25,.75,.5,.5,.25,.25,.25,0,0,0,.75] ]
channels=['diffuse_texture','normalmap_texture','tangent_texture','height_texture','reflectionroughness_texture','metallic_texture',
          'emissive_mask_texture','subsurface_transmittance_texture','subsurface_thickness_texture','subsurface_single_scattering_texture','subsurface_radius_texture','secondary_texture']
constants=dict(anisotropy=.3,diffuse_color_constant=[.2,.4,.6],opacity_constant=.75,reflection_roughness_constant=.35,metallic_constant=.6,
    emissive_intensity=2.,emissive_color_constant=[.1,.3,.5],enable_emission=True,sprite_sheet_rows=3,sprite_sheet_cols=4,sprite_sheet_fps=9,
    enable_thin_film=True,thin_film_thickness_constant=320.,thin_film_thickness_from_albedo_alpha=False,use_legacy_alpha_state=True,
    blend_enabled=True,blend_type=0,inverted_blend=True,alpha_test_type=7,alpha_test_reference_value=97,displace_in=.04,displace_out=.02,
    subsurface_transmittance_color=[.2,.5,.7],subsurface_measurement_distance=.6,subsurface_single_scattering_albedo=[.3,.4,.8],
    subsurface_volumetric_anisotropy=-.2,subsurface_diffusion_profile=True,subsurface_radius=[.1,.2,.4],subsurface_radius_scale=.75,
    subsurface_max_sample_radius=1.5,filter_mode=1,wrap_mode_u=1,wrap_mode_v=2)
try:
    execution=json.loads((run/'execution.json').read_text(encoding='utf-8-sig'))
    check('CPU exporter completed without forced cleanup',execution['exitCode']==0 and execution['signaled'] and not execution['forced']
          and not execution['memoryLimited'] and not execution['timedOut'] and not execution['remaining'])
    stdout=(run/'stdout.log').read_text(encoding='utf-8')
    check('four scenes and no GPU runtime',stdout.count('EXPORTED ')==4 and json.loads(stdout.splitlines()[-1])['gpuRuntimeLoaded'] is False)
    stderr=(run/'stderr.log').read_text(encoding='utf-8',errors='replace')
    check('no USD coding error or error dialog',not any(s in stderr for s in ('Coding Error','OWN_NONINTERACTIVE_EXPORT_ERROR','Capture failed')))
    for nonuniform in (False,True):
      for reduced in (False,True):
        name=('nonuniform' if nonuniform else 'uniform')+('-reduced' if reduced else '-full')
        directory=run/'captures'/name
        scene=dict(name=name,meshes=[],samples=[]);report['scenes'].append(scene)
        stage=Usd.Stage.Open(str(directory/'own_offline.usda'))
        check(name+' stage metadata',bool(stage) and stage.GetStartTimeCode()==0 and stage.GetEndTimeCode()==(0 if options.single_frame else 2) and stage.GetTimeCodesPerSecond()==24
              and UsdGeom.GetStageUpAxis(stage)=='Y' and UsdGeom.GetStageMetersPerUnit(stage)==1 and str(stage.GetDefaultPrim().GetPath())=='/RootNode')
        camera=UsdGeom.Camera(stage.GetPrimAtPath('/RootNode/cameras/Camera'))
        ops=UsdGeom.Xformable(camera).GetOrderedXformOps()
        check(name+' camera sample mode',len(ops)==1 and ops[0].GetAttr().GetTimeSamples()==([] if options.single_frame else [0.,1.,2.]))
        for t in range(1 if options.single_frame else 3):near(name+' camera '+str(t),value(ops[0].Get(t)),value(Gf.Matrix4d(1)))
        layers=[l for l in stage.GetUsedLayers() if not l.anonymous]
        scene['layers']=[l.realPath for l in layers]
        check(name+' ten contained composition layers',len(layers)==10 and all(Path(l.realPath).is_file() and Path(l.realPath).resolve().is_relative_to(directory.resolve()) for l in layers))
        cache=UsdSkel.Cache()
        skel_roots=[p for p in stage.Traverse() if p.IsA(UsdSkel.Root)]
        check(name+' twelve skeleton roots',len(skel_roots)==12)
        for p in skel_roots:check(name+str(p.GetPath())+' populate',cache.Populate(UsdSkel.Root(p),Usd.PrimDefaultPredicate))
        shader=stage.GetPrimAtPath('/RootNode/Looks/mat_own_opaque/Shader')
        check(name+' one material',len([p for p in stage.Traverse() if p.IsA(UsdShade.Material)])==1)
        scene['material']={a.GetName():value(a.Get()) for a in shader.GetAttributes() if a.HasAuthoredValueOpinion()}
        for k,want in constants.items():
            attr=shader.GetAttribute('inputs:'+k)
            near(name+' material '+k,value(attr.Get()),want)
            expected_type='bool' if isinstance(want,bool) else 'uint' if isinstance(want,int) else 'float3' if isinstance(want,list) else 'float'
            check(name+' material '+k+' type',str(attr.GetTypeName())==expected_type)
        assets={a.GetName():a.Get() for a in shader.GetAttributes() if a.GetName().startswith('inputs:') and isinstance(a.Get(),Sdf.AssetPath)}
        check(name+' twelve material channels',set(assets)=={'inputs:'+c for c in channels})
        for i,c in enumerate(channels):
            asset=assets.get('inputs:'+c);present=bool(asset and asset.resolvedPath)
            path=Path(asset.resolvedPath) if present else Path()
            check(name+' '+c+' relative contained reference',present and path.is_file() and path.resolve().is_relative_to(directory.resolve()) and not Path(asset.path).is_absolute())
            if present:
                data=path.read_bytes()
                check(name+' '+c+' own DDS data',len(data)==132 and data[:4]==b'DDS ' and struct.unpack_from('<II',data,12)==(1,1)
                      and list(data[128:])==[96+4*i,64+8*i,32+16*i,192])
        mdl=shader.GetAttribute('info:mdl:sourceAsset').Get()
        check(name+' contained MDL',bool(mdl.resolvedPath) and Path(mdl.resolvedPath).resolve().is_relative_to(directory.resolve()))
        for prim in stage.Traverse():
            if not prim.IsA(UsdGeom.Mesh):continue
            path=str(prim.GetPath());label=name+path
            match=re.search(r'(?:mesh_|inst_)case([1-4])',path);assert match,path
            n=int(match[1]);c=n-1
            logical=[[1.25+(-.5 if i%2==0 else .5),(c-1.5)*.75+(.25 if i<2 else -.25),5.] for i in range(4)]
            slots=[None,1,3,None,0,2] if not reduced else [1,3,0,2]
            expected_points=[logical[i] if i is not None else [99.,99.,99.] for i in slots]
            expected_weights=[x for i in slots for x in ([abs(w) for w in weights[c][i*n:(i+1)*n]]+[0.] if i is not None else [0.]*(n+1))]
            expected_joints=[j for i in slots for j in ([(i+k)%4+(4 if weights[c][i*n+k]<0 else 0) for k in range(n)]+[8] if i is not None else [0]*n+[8])]
            mesh=UsdGeom.Mesh(prim);pv=UsdGeom.PrimvarsAPI(prim);binding=UsdSkel.BindingAPI(prim)
            points=value(mesh.GetPointsAttr().Get());js=binding.GetJointIndicesPrimvar();ws=binding.GetJointWeightsPrimvar()
            scene['meshes'].append(dict(path=path,case=n,points=points,weights=value(ws.Get()),joints=value(js.Get())))
            near(label+' source geometry',points,expected_points)
            check(label+' topology',value(mesh.GetFaceVertexCountsAttr().Get())==[3,3] and value(mesh.GetFaceVertexIndicesAttr().Get())==([2,0,3,3,0,1] if reduced else [4,1,5,5,1,2]))
            check(label+' handedness and sidedness',mesh.GetOrientationAttr().Get()=='rightHanded' and mesh.GetDoubleSidedAttr().Get())
            near(label+' normals',value(mesh.GetNormalsAttr().Get()),[[0,0,-1]]*len(slots))
            near(label+' UV',value(pv.GetPrimvar('st').Get()),[[i%2,i//2] if i is not None else [0,0] for i in slots])
            near(label+' color',value(pv.GetPrimvar('displayColor').Get()),[[.25,.5,.75]]*len(slots))
            near(label+' opacity',value(pv.GetPrimvar('displayOpacity').Get()),[.625]*len(slots))
            near(label+' explicit weights',value(ws.Get()),expected_weights,0)
            check(label+' indices and influence width',value(js.Get())==expected_joints and ws.GetElementSize()==js.GetElementSize()==n+1 and ws.GetInterpolation()==js.GetInterpolation()=='vertex')
            material,_=UsdShade.MaterialBindingAPI(prim).ComputeBoundMaterial()
            check(label+' bound material',str(material.GetPath())=='/RootNode/Looks/mat_own_opaque')
            check(label+' material binding schema applied',prim.GetParent().HasAPI(UsdShade.MaterialBindingAPI))
            skeleton=binding.GetInheritedSkeleton();query=cache.GetSkelQuery(skeleton);skin=cache.GetSkinningQuery(prim)
            expected_names=['root','root/joint1','root/joint2','root/joint3'] if options.topology=='star' else ['joint'+str(j) for j in range(9)]
            check(label+' palette and queries',bool(query) and bool(skin) and list(query.GetJointOrder())==expected_names)
            prototype='/meshes/' in path;late='_late/' in path
            for t in range(1 if options.single_frame else 3):check(label+' visibility '+str(t),(UsdGeom.Imageable(prim).ComputeVisibility(t)=='inherited')==(not prototype and (options.single_frame or not late or t==1)))
            if prototype:continue
            parent=prim.GetParent();op=UsdGeom.Xformable(parent).GetOrderedXformOps()
            check(label+' single instance transform op',len(op)==1 and op[0].GetAttr().GetTimeSamples()==([] if options.single_frame else [1.] if late else [0.,1.,2.]))
            anim=query.GetAnimQuery();check(label+' animation bound',bool(anim))
            anim_schema=UsdSkel.Animation(anim.GetPrim())
            for attr in (anim_schema.GetTranslationsAttr(),anim_schema.GetRotationsAttr(),anim_schema.GetScalesAttr()):
                check(label+' '+attr.GetName()+' sample times',attr.GetTimeSamples()==([] if options.single_frame else [1.] if late else [0.,1.,2.]))
            for t in ([0] if options.single_frame else [1] if late else range(3)):
                phase=1 if options.single_frame else t
                prefix=label+' t'+str(t)
                local=Gf.Matrix4d(1);local.SetTranslate(Gf.Vec3d(.125*phase,.25 if late else 0,0))
                near(prefix+' instance matrix',value(op[0].Get(t)),value(local))
                expected_palette=adapter_palette(phase,nonuniform)
                actual=query.ComputeSkinningTransforms(t)
                near(prefix+' palette reconstruction',value(actual),expected_palette)
                deformed=Vt.Vec3fArray([Gf.Vec3f(*p) for p in points])
                check(prefix+' UsdSkel skin operation',skin.ComputeSkinnedPoints(actual,deformed,t))
                want=[]
                for i in slots:
                    if i is None:want.append([0.,0.,0.]);continue
                    transformed=[transform(logical[i],expected_palette[(i+k)%4]) for k in range(n)]
                    want.append([sum(weights[c][i*n+k]*transformed[k][a] for k in range(n)) for a in range(3)])
                near(prefix+' skinned vertices',value(deformed),want)
                scene['samples'].append(dict(path=path,time=t,palette=value(actual),expectedPalette=expected_palette,
                    deformed=value(deformed),expectedDeformed=want,paletteError=error(value(actual),expected_palette),pointError=error(value(deformed),want)))
        check(name+' twelve meshes',len(scene['meshes'])==12)
        scene['outputHashes']={p.relative_to(directory).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.rglob('*') if p.is_file()}
    report['status']='PASS' if all(c['passed'] for c in report['checks']) else 'FAIL'
except Exception:report['error']=traceback.format_exc()
report['scope']='Own CPU production signed adapter: actual USD export/readback, B2..5 influences, 9-joint positive/negative/zero palette, animated scales/rotations, reduction, visibility, 33 constants and 12 own DDS references. No GPU/capturer/texture writer/game qualification.'
report['jointIndices']='Production EncodeSignedSkin: B1..4 to B2..5, positive/negative palette plus zero bone; canonical final explicit weight0'
report['singleFrame']=options.single_frame
report['adapterScope']='Production encoder and actual exporter; independent original signed-weight deformation. No capturer/GPU/roundtrip render.'
with output.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
failed=[c for c in report['checks'] if not c['passed']]
print(json.dumps(dict(status=report['status'],checks=len(report['checks']),failed=len(failed),firstFailures=failed[:8],error=report.get('error'),
    scenes=[dict(name=s['name'],samples=len(s['samples']),maxPalette=max([x['paletteError'] for x in s['samples']]+[0]),maxPoint=max([x['pointError'] for x in s['samples']]+[0])) for s in report['scenes']])))
sys.exit(0 if report['status']=='PASS' else 1)
