"""Production GLB/FBX -> independent Blender importers -> PRS-backed vertex FK.

The four rare clips use original-PC PRS at every 30fps frame. Source bind,
geometry and palette are shared export inputs already validated separately;
this test measures animation application, not a new native mesh/skin decoder.
"""
from pathlib import Path
import hashlib
import json
import sys
import time

import bpy
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from san_pose_reference import expected


def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def capture(path, samples, reference):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = 30
    if path.suffix == '.fbx': bpy.ops.import_scene.fbx(filepath=str(path.resolve()), anim_offset=0)
    else: bpy.ops.import_scene.gltf(filepath=str(path.resolve()), merge_vertices=False)
    rows = []
    for sample in samples:
        frame = sample['seconds']*30
        scene.frame_set(int(frame), subframe=frame-int(frame))
        graph = bpy.context.evaluated_depsgraph_get()
        source = expected(reference, sample['tracks'])
        extent = float(np.ptp(np.concatenate(list(source.values())), axis=0).max(initial=0))
        tolerance = 1e-5 + extent*8e-6
        for name, wanted in source.items():
            obj = bpy.data.objects.get(name)
            assert obj is not None and obj.type == 'MESH', f'Missing placement {name}'
            evaluated = obj.evaluated_get(graph)
            mesh = evaluated.to_mesh()
            try:
                coordinates = np.empty(len(mesh.vertices)*3, dtype=np.float32)
                mesh.vertices.foreach_get('co', coordinates)
                matrix = np.array(evaluated.matrix_world, dtype=np.float64)
                actual = coordinates.reshape(-1, 3).astype(np.float64) @ matrix[:3, :3].T + matrix[:3, 3]
            finally:
                evaluated.to_mesh_clear()
            assert actual.shape == wanted.shape, f'Vertex shape/order: {name}, {actual.shape} != {wanted.shape}'
            error = float(np.linalg.norm(actual-wanted, axis=1).max(initial=0))
            rows.append({'seconds': sample['seconds'], 'mesh': name, 'vertices': len(wanted),
                         'receiverSubframeDiagnostic': sample.get('receiverSubframeDiagnostic', False),
                         'maximumError': error, 'sceneExtent': extent, 'tolerance': tolerance, 'passed': error <= tolerance})
    return rows


def main():
    source, destination = map(Path, sys.argv[sys.argv.index('--')+1:])
    data = json.loads(source.read_text(encoding='utf-8-sig'))
    started = time.perf_counter()
    report = {'status': 'running', 'inputSha256': digest(source), 'blenderVersion': bpy.app.version_string, 'cases': []}
    try:
        for case in data['cases']:
            reference = source.parent/case['reference']
            assert digest(reference) == case['referenceSha256']
            inputs = json.loads(reference.read_text(encoding='utf-8-sig'))
            result = {'name': case['name'], 'nativePrsReference': case['nativePrsReference'], 'formats': []}
            report['cases'].append(result)
            for extension in ('glb', 'fbx'):
                path = source.parent/case[extension]
                assert digest(path) == case[extension+'Sha256']
                rows = capture(path, case['samples'], inputs)
                result['formats'].append({'format': extension, 'rows': rows,
                                           'maximumError': max(row['maximumError'] for row in rows)})
            print('Measured', case['name'], [(row['format'], row['maximumError']) for row in result['formats']], flush=True)
        rows = [row for case in report['cases'] for format_row in case['formats'] for row in format_row['rows']]
        report['requiredChecks'] = sum(not row['receiverSubframeDiagnostic'] for row in rows)
        report['receiverSubframeMismatches'] = sum(row['receiverSubframeDiagnostic'] and not row['passed'] for row in rows)
        assert all(row['passed'] for row in rows if not row['receiverSubframeDiagnostic']), 'Target vertices differ from PRS-backed FK; inspect per-mesh errors.'
        report['status'] = 'passed-with-receiver-subframe-limit' if report['receiverSubframeMismatches'] else 'passed'
    except Exception as error:
        report['status'], report['error'] = 'failed', repr(error)
        raise
    finally:
        report['elapsedSeconds'] = time.perf_counter()-started
        destination.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(report['status'], 'production GLB/FBX shared SAN against PRS-backed vertex FK;',
          report['requiredChecks'], 'required checks;', report['receiverSubframeMismatches'], 'declared receiver subframe mismatches')


if __name__ == '__main__': main()
