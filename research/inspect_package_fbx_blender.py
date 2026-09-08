"""Independent delivery check of the FBX emitted by the packaged GUI.

This confirms scene/skin/clip availability and finite changing geometry. It
does not replace the CP128 numeric comparisons against original PC curves.
"""
from pathlib import Path
import hashlib
import json
import sys
import time

import bpy
import numpy as np


def main():
    source, destination = map(Path, sys.argv[sys.argv.index('--')+1:])
    started=time.perf_counter()
    report={'status':'running','blenderVersion':bpy.app.version_string}
    try:
        handoff=json.loads(source.read_text(encoding='utf-8'))
        assert handoff['status']=='passed'
        path=Path(handoff['fbxExport']['path'])
        assert hashlib.sha256(path.read_bytes()).hexdigest().upper()==handoff['fbxExport']['sha256']
        report['fbxSha256']=handoff['fbxExport']['sha256']
        bpy.ops.wm.read_factory_settings(use_empty=True)
        scene=bpy.context.scene
        scene.render.fps=30
        bpy.ops.import_scene.fbx(filepath=str(path),anim_offset=0)
        meshes=sorted((obj for obj in scene.objects if obj.type=='MESH'),key=lambda obj:obj.name)
        report['meshes']=[{'name':obj.name,'vertices':len(obj.data.vertices),'triangles':sum(len(poly.vertices)-2 for poly in obj.data.polygons),
                           'armatureModifiers':sum(mod.type=='ARMATURE' and mod.object is not None for mod in obj.modifiers)} for obj in meshes]
        assert len(meshes)==6
        assert sum(row['vertices'] for row in report['meshes'])==5280
        assert sum(row['triangles'] for row in report['meshes'])==1880
        assert all(row['armatureModifiers']==1 for row in report['meshes'])
        report['armatures']=[{'name':obj.name,'bones':len(obj.data.bones)} for obj in scene.objects if obj.type=='ARMATURE']
        assert report['armatures']
        report['actions']=[{'name':action.name,'frameRange':list(action.frame_range)} for action in bpy.data.actions]
        assert any('blwalk' in row['name'] for row in report['actions'])
        report['images']=[{'name':img.name,'width':img.size[0],'height':img.size[1],'channels':img.channels} for img in bpy.data.images]
        assert report['images'] and all(row['width']>0 and row['height']>0 for row in report['images'])
        poses=[]
        for frame in [0,15,29]:
            scene.frame_set(frame)
            graph=bpy.context.evaluated_depsgraph_get()
            rows=[]
            for obj in meshes:
                evaluated=obj.evaluated_get(graph)
                mesh=evaluated.to_mesh()
                try:
                    values=np.empty(len(mesh.vertices)*3,dtype=np.float32)
                    mesh.vertices.foreach_get('co',values)
                    matrix=np.array(evaluated.matrix_world,dtype=np.float64)
                    points=values.reshape(-1,3).astype(np.float64)@matrix[:3,:3].T+matrix[:3,3]
                    assert np.isfinite(points).all()
                    rows.append(points)
                finally:evaluated.to_mesh_clear()
            poses.append(np.concatenate(rows))
        movement=[float(np.linalg.norm(pose-poses[0],axis=1).max()) for pose in poses[1:]]
        report['sampleFrames']=[0,15,29]
        report['sampledVertexPositions']=sum(len(pose) for pose in poses)
        report['maximumVertexMotionFromStart']=movement
        assert all(value>1e-4 for value in movement)
        report['status']='passed'
    except Exception as exc:
        report.update(status='failed',error=repr(exc))
        raise
    finally:
        report['elapsedSeconds']=time.perf_counter()-started
        report['limitations']=['One packaged GUI FBX, imported through Blender; finite changing poses and delivery inventory only.',
                              'Not a new original-PC curve/vertex comparison or visual quality evaluation.']
        destination.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))


if __name__=='__main__':main()
