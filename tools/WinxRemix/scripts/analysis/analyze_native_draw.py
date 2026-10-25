"""Join sampled native renderer state, D3D inputs and verified shader identity.

Own diagnostic grouping only; no duplicate material/light-selection evaluator.
Incomplete logs and missing links are reported rather than treated as coverage.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
import os
from pathlib import Path


def records(path):
    if path.stat().st_size > 34 * 1024 * 1024:
        raise ValueError(f'Log exceeds observer budget: {path}')
    with path.open(encoding='utf-8-sig') as stream:
        for line in stream:
            if not line.endswith('\n'):
                raise ValueError(f'Incomplete log tail: {path}')
            yield json.loads(line)


def analyze(run):
    # WM_CLOSE only queues shutdown. Do not publish a final report while the
    # game can still append another sampled frame after that window command.
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    if os.name == 'nt':
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        process = kernel.OpenProcess(0x1000, False, launch['pid'])
        if process:
            try:
                code = wintypes.DWORD()
                if not kernel.GetExitCodeProcess(process, ctypes.byref(code)):
                    raise ctypes.WinError(ctypes.get_last_error())
                if code.value == 259:
                    raise ValueError('Recorded game PID is still active; wait for completed shutdown')
            finally:
                kernel.CloseHandle(process)
        elif ctypes.get_last_error() != 87:  # exited PID: ERROR_INVALID_PARAMETER
            raise ctypes.WinError(ctypes.get_last_error())
    observed_paths = [run / name for name in ('materials.jsonl', 'native-draws.jsonl', 'shader-semantics.jsonl')
                      if (run / name).exists()]
    stamps = {p: (p.stat().st_size, p.stat().st_mtime_ns) for p in observed_paths}
    inputs, material_draws, contexts, shader_draws = {}, {}, {}, {}
    native_errors = Counter()
    material_errors = Counter()
    for row in records(run / 'materials.jsonl'):
        if row['event'] == 'input':
            inputs[row['id']] = row['data']
        elif row['event'] == 'sample':
            key = row['frame'], row['draw']
            if key in material_draws or len(material_draws) >= 300000:
                raise ValueError('Duplicate sample or draw budget exceeded')
            material_draws[key] = row['input']
        elif row['event'] == 'frame':
            material_errors.update({k: row[k] for k in ('failures', 'rejected')})
    shader_path = run / 'shader-semantics.jsonl'
    if shader_path.exists():
        for row in records(shader_path):
            if row['event'] == 'draw':
                shader_draws[row['frame'], row['draw']] = row
    groups, checks, formats, modes, lights, pass_counts = [Counter() for _ in range(6)]
    combiners = Counter()
    frames = defaultdict(Counter)
    native_materials, controllers = set(), set()
    joined = missing = draw_count = 0
    for row in records(run / 'native-draws.jsonl'):
        kind = row['event']
        if kind == 'context':
            if row['id'] in contexts or len(contexts) >= 4096:
                raise ValueError('Duplicate context or context budget exceeded')
            contexts[row['id']] = row['data']
        elif kind == 'frame':
            native_errors.update({k: row[k] for k in ('failures', 'limited')})
        elif kind == 'draw':
            draw_count += 1
            if draw_count > 300000:
                raise ValueError('Native draw budget exceeded')
            key = row['frame'], row['draw']
            data = contexts[row['context']]
            if key not in material_draws:
                missing += 1
                continue
            joined += 1
            d3d = inputs[material_draws[key]]
            states = dict(d3d['states'])
            world = d3d['perspective'] and d3d['primaryTarget'] and states.get(7, 0) != 0
            scope = 'primary_perspective_depth' if world else 'other'
            mode = data['effectiveStates'][8]
            vs = d3d['vertexShader']
            path = 'vertex_shader' if vs else 'ffp_lit' if states[137] else 'ffp_unlit'
            groups[scope, path, mode, bool(data['activeBones'])] += 1
            if not world:
                continue
            frames[row['frame']][f'{path}:mode{mode}'] += 1
            native_materials.add(data['selectedMaterial'])
            if data.get('colorController'):
                controllers.add(data['colorController'])
            for check in ('materialValid', 'cachedMaterialEqualsDevice', 'nativeVBValid', 'nativeVBEqualsDevice'):
                checks[f'{check}:{data[check]}'] += 1
            checks[f'selectedEqualsInstalled:{data["selectedMaterial"] == data["installedMaterial"]}'] += 1
            source_mode = data.get('sourceStates', [None] * 9)[8]
            modes[source_mode, mode, bool(data['overrideTableActive'])] += 1
            ordinary = data.get('lights', [])
            ambient = data.get('ambientLight')
            lights[path, len(ordinary), bool(ambient)] += 1
            checks['invalidSelectedLight'] += sum(not light or not light['valid'] for light in ordinary)
            checks['invalidAmbientLight'] += bool(ambient and not ambient['valid'])
            checks['unlitWithSelectedLight'] += path == 'ffp_unlit' and bool(ordinary or ambient)
            pass_counts[len(data.get('passes', []))] += 1
            active_stages = []
            for stage in d3d['stages']:
                if stage[0] == 1:  # observed D3DTOP_DISABLE terminates FFP stages
                    break
                active_stages.append(tuple(stage))
            combiners[path, states[27], states[15], tuple(active_stages)] += 1
            checks['invalidPass'] += sum(not p['valid'] for p in data.get('passes', []))
            layout = tuple(tuple(element) for element in data['declaration'])
            formats[path, mode, data['activeBones'], data['stride'], layout] += 1
            if vs:
                link = shader_draws.get(key)
                linked = bool(link and link.get('bytecodeEqual'))
                checks[f'verifiedShaderLink:{linked}'] += 1
                if linked:
                    # This compares the recorded key field to the native state;
                    # BuildShaderKey/Fixed.rfx semantics stay in common sources.
                    checks[f'shaderKeyModeMatches:{(link["key"][0] >> 16 & 15) == mode}'] += 1
    paths = ['launch.json', 'materials.jsonl', 'native-draws.jsonl']
    if shader_path.exists():
        paths.append(shader_path.name)
    if any((p.stat().st_size, p.stat().st_mtime_ns) != stamp for p, stamp in stamps.items()):
        raise ValueError('Audit files changed during analysis')
    return dict(schema=1, run=str(run), closedProcessChecked=os.name == 'nt',
                scope='Joined sampled draws; perspective/depth is a pass filter, not native owner/camera proof',
                nativeDraws=draw_count, joinedDraws=joined, missingMaterialLinks=missing,
                nativeErrors=dict(native_errors), materialErrors=dict(material_errors),
                contexts=len(contexts), uniqueWorldMaterialAddresses=len(native_materials),
                worldColorControllerAddresses=sorted(controllers), checks=dict(checks),
                groups=[dict(scope=s, path=p, mode=m, skinned=b, draws=n)
                        for (s, p, m, b), n in sorted(groups.items())],
                modes=[dict(source=s, effective=e, override=o, draws=n)
                       for (s, e, o), n in sorted(modes.items(), key=lambda item: str(item[0]))],
                selectedLights=[dict(path=p, ordinary=o, ambient=a, draws=n)
                                for (p, o, a), n in sorted(lights.items())],
                passCounts=dict(pass_counts),
                combinerContracts=[dict(path=p, alphaBlend=b, alphaTest=t, stages=s, draws=n)
                                   for (p, b, t, s), n in combiners.most_common()],
                formats=[dict(path=p, mode=m, bones=b, stride=s, declaration=d, draws=n)
                         for (p, m, b, s, d), n in formats.most_common()],
                frames=[dict(frame=f, draws=dict(c)) for f, c in sorted(frames.items())],
                files=[dict(name=name, bytes=(run/name).stat().st_size,
                            sha256=hashlib.sha256((run/name).read_bytes()).hexdigest()) for name in paths],
                limits=['No draw ownership inferred from lightCache address',
                        'Native pointers identify observations within one run, not stable asset identities',
                        'Passes are candidate lists, not an inferred active pass for multi-pass materials',
                        'No inference that vertex color can be separated into baked light and albedo',
                        'No normal synthesis, CPU skinning or physical light conversion in this analyzer'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = analyze(args.run)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
    print(json.dumps({k: report[k] for k in ('nativeDraws', 'joinedDraws', 'missingMaterialLinks',
                                            'nativeErrors', 'materialErrors', 'checks', 'groups')}))
