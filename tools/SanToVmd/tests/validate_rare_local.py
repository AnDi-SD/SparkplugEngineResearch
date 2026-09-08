"""Selected real clips plus native-backed rare SAN -> portable CLI -> MMD reader.

Developer-only local validation. Four ordinary clips exercise walking, running,
idle and hit. Three PMDs cover ordinary, extra-parent and duplicate-name rigs.
Four synthetic SANs have original-PC PRS for every output frame; no game assets
or generated VMDs are placed in Git. Run native probe first.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

import validate_local

converter, ROOT = validate_local.converter, validate_local.ROOT
ORDINARY = ('xiwa.san', 'xiru.san', 'xiid.san', 'xibhu.san')
MODELS = ('Miku_Hatsune_Ver2.pmd', 'Luka_Megurine.pmd', 'MEIKO.pmd')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def read_vmd(external, path):
    with path.open('rb') as stream:
        header, bones = external.Header(), external.BoneAnimation()
        header.load(stream)
        bones.load(stream)
        rest = [external.ShapeKeyAnimation(), external.CameraAnimation(), external.LightAnimation(),
                external.SelfShadowAnimation(), external.PropertyAnimation()]
        for section in rest:
            section.load(stream)
        assert stream.tell() == path.stat().st_size
    return header, bones, rest


def native_world(source, order, tracks):
    """Column-matrix FK from original PRS; never calls converter.sample/rotate."""
    result = {}
    identity = [[int(i == j) for j in range(4)] for i in range(4)]
    for name in order:
        bone, channels = source[name], tracks.get(name, {})
        position = channels.get('2', bone.position)
        x, y, z, w = channels.get('3', bone.rotation)
        scale = channels.get('4', bone.scale)
        assert all(abs(v-1) < .001 for v in scale)
        local = [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w), position[0]],
                 [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w), position[1]],
                 [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y), position[2]],
                 [0, 0, 0, 1]]
        parent = result.get(bone.parent, identity)
        result[name] = [[sum(parent[i][k]*local[k][j] for k in range(4))
                         for j in range(4)] for i in range(4)]
    return {name: tuple(row[i][3] for i in range(3)) for name, row in result.items()}


def compare_old(old, new):
    assert set(old) == set(new)
    rotation, position, changed = 0.0, 0.0, 0
    for name, frames in new.items():
        assert len(frames) == len(old[name])
        for current, previous in zip(frames, old[name]):
            assert current.frame_number == previous.frame_number
            a, b = current.rotation, previous.rotation
            cosine = abs(sum(x*y for x, y in zip(a, b)))
            cosine /= math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
            rotation = max(rotation, 2*math.acos(min(1.0, cosine)))
            position = max(position, math.sqrt(sum((x-y)**2 for x, y in zip(current.location, previous.location))))
            changed += tuple(a) != tuple(b) or tuple(current.location) != tuple(previous.location)
    return {'changed_keys': changed, 'max_rotation_radians': rotation, 'max_position_units': position}


def validate(external, path, source_path, model_path, native_fixture, baseline):
    source = converter.read_skeleton(source_path)
    model_name, target, iks = converter.read_pmd(model_path)
    rig = converter.Retargeter(source, target, iks)
    clip = converter.read_san(path, set(rig.order))
    header, bones, rest = read_vmd(external, baseline['new'])
    assert header.model_name == model_name
    assert set(bones) == set(rig.mapping) | {'センター'} | set(rig.neutral_bones)
    assert not any(rest[:-1])
    assert len(rest[-1]) == 1 and rest[-1][0].frame_number == 0 and rest[-1][0].visible
    assert dict(rest[-1][0].ik_states) == {name: False for name in rig.disabled_ik}
    count = converter.frame_count(clip.duration)
    keys = 0
    for name, frames in bones.items():
        assert [f.frame_number for f in frames] == list(range(count))
        previous = None
        for key in frames:
            assert all(math.isfinite(v) for v in (*key.location, *key.rotation))
            assert abs(sum(v*v for v in key.rotation)-1) < 1e-5
            if previous is not None:
                assert sum(a*b for a, b in zip(previous, key.rotation)) >= -1e-6
            previous = key.rotation
            if name in rig.neutral_bones:
                assert tuple(key.location) == converter.ZERO and tuple(key.rotation) == converter.IDENTITY
            keys += 1
    maximum = 0.0
    center_error = 0.0
    frames = range(count) if native_fixture else sorted({0, count//2, count-1})
    for frame in frames:
        posed = validate_local.mmd_world(target, bones, frame)
        if native_fixture:
            sample = native_fixture['samples'][frame]
            assert sample['frame'] == frame
            original = native_world(source, rig.order, sample['tracks'])
        else:
            original = {name: value[0] for name, value in
                        converter.source_world(source, rig.order, clip, min(frame/30, clip.duration)).items()}
        expected_center = tuple((a-b)*rig.scale for a, b in zip(original['Pelvis'], rig.rest['Pelvis'][0]))
        center_error = max(center_error, max(abs(a-b) for a, b in zip(bones['センター'][frame].location, expected_center)))
        assert center_error < 2e-5
        for side in ('左', '右'):
            for first, second in (('肩', '腕'), ('腕', 'ひじ'), ('ひじ', '手首'), ('足', 'ひざ'), ('ひざ', '足首')):
                a, b = side+first, side+second
                desired = converter.sub(original[rig.mapping[b]], original[rig.mapping[a]])
                actual = converter.sub(posed[b], posed[a])
                desired, actual = converter.times(desired, 1/converter.length(desired)), converter.times(actual, 1/converter.length(actual))
                error = converter.length(converter.sub(actual, desired))
                maximum = max(maximum, error)
                assert error < .002, (path.name, model_path.name, frame, a, error)
    row = {'san': path.name, 'san_sha256': digest(path), 'model': model_path.name, 'model_sha256': digest(model_path),
           'vmd_sha256': digest(baseline['new']), 'frames': count, 'keys': keys, 'checked_poses': len(frames),
           'native_prs_reference': native_fixture is not None, 'max_limb_direction_error': maximum,
           'max_center_error': center_error}
    if baseline.get('old'):
        _, old, _ = read_vmd(external, baseline['old'])
        row['previous_version_comparison'] = compare_old(old, bones)
        row['previous_vmd_sha256'] = digest(baseline['old'])
    clip.close()
    rig.close()
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-report', type=Path, required=True)
    parser.add_argument('--reader', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):
        raise ValueError('Results must stay under local-data/results')
    output.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    native = json.loads(args.native_report.read_text(encoding='utf-8'))
    assert native['status'] == 'passed'
    # Original-PC samples remain valid when the consumer changes. Reuse only
    # the same executable and fixture bytes, both checked by SHA256 below.
    assert native['pc_sha256'] == digest(ROOT/'local-data/pc-pristine/WinxClub.exe')
    samples = {row['san']: row for row in native['vmd_fixtures']}
    spec = importlib.util.spec_from_file_location('external_vmd', args.reader)
    external = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(external)
    source = ROOT/'local-data/pc-pristine/Media/Characters/Icy'
    models = ROOT/'local-data/mmd/MikuMikuDanceE_v932/UserFile/Model'
    old_commit = '591b75e'
    old_script = subprocess.check_output(['git', 'show', f'{old_commit}:tools/SanToVmd/san_to_vmd.py'], cwd=ROOT, timeout=10)
    results = []
    with tempfile.TemporaryDirectory(prefix='vmd-rare-', dir=ROOT/'.codex-tmp') as temporary:
        base = Path(temporary)
        for version in ('new', 'old'):
            package = base/version
            directory = package/'input'
            directory.mkdir(parents=True)
            (package/'san_to_vmd.py').write_bytes(Path(converter.__file__).read_bytes() if version == 'new' else old_script)
            shutil.copyfile(source/'Icy.smo', directory/'Icy.smo')
            for name in ORDINARY:
                shutil.copyfile(source/name, directory/name)
            if version == 'new':
                validate_local.copy_runtime(package)
                for name, row in samples.items():
                    path = args.native_report.parent/'vmd-fixtures'/name
                    assert digest(path) == row['sha256']
                    shutil.copyfile(path, directory/name)
        for model in MODELS:
            for version in ('new', 'old'):
                package = base/version
                shutil.copyfile(models/model, package/'input/reference.pmd')
                process = subprocess.run([sys.executable, '-X', 'utf8', str(package/'san_to_vmd.py')],
                                         cwd=base, capture_output=True, text=True, encoding='utf-8', timeout=30)
                assert process.returncode == 0, (model, version, process.stdout, process.stderr)
                shutil.copytree(package/'output', output/model[:-4]/version, dirs_exist_ok=True)
            for name in (*ORDINARY, *samples):
                baseline = {'new': output/model[:-4]/'new'/(Path(name).stem+'.vmd')}
                if name in ORDINARY:
                    baseline['old'] = output/model[:-4]/'old'/(Path(name).stem+'.vmd')
                results.append(validate(external, base/'new/input'/name, source/'Icy.smo', models/model, samples.get(name), baseline))
            print(f'PASS {model}: 4 ordinary + 4 original-PC-backed rare clips', flush=True)
    report = {'status': 'passed', 'version': converter.VERSION, 'seconds': time.perf_counter()-started,
              'converter_sha256': digest(Path(converter.__file__)), 'validator_sha256': digest(Path(__file__)),
              'native_dll_sha256': digest(Path(converter.native.library()._name)),
              'adapter_sha256': digest(Path(converter.native.__file__)),
              'original_reference_converter_sha256': native['converter_sha256'],
              'native_report_sha256': digest(args.native_report), 'independent_reader_sha256': digest(args.reader),
              'previous_version_commit': old_commit, 'previous_converter_sha256': hashlib.sha256(old_script).hexdigest().upper(),
              'skeleton_sha256': digest(source/'Icy.smo'), 'portable_cli_unrelated_cwd': True,
              'vmd_files': len(results), 'keys': sum(row['keys'] for row in results),
              'native_reference_pose_checks': sum(row['checked_poses'] for row in results if row['native_prs_reference']),
              'max_limb_direction_error': max(row['max_limb_direction_error'] for row in results),
              'max_center_error': max(row['max_center_error'] for row in results), 'results': results,
              'limits': ['frame-sampled 30fps VMD', 'three documented PMD profiles', 'unit scale only',
                         'no original MMD visual playback or mesh/physics claim',
                         'ordinary pose reference uses converter; rare pose reference uses original-PC PRS']}
    (output/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('status', 'vmd_files', 'keys', 'native_reference_pose_checks', 'max_limb_direction_error', 'seconds')}))


if __name__ == '__main__':
    main()
