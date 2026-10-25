"""Summarize sampled original material inputs, without claiming GPU equivalence."""
import argparse
import collections
import json
from pathlib import Path
import re


def analyze(run, verify_shaders=False):
    inputs, shaders = {}, {}
    counts = collections.Counter()
    references = collections.Counter()
    frames = set()
    failed = rejected = 0
    incomplete_tail = False
    path = run / 'materials.jsonl'
    with path.open('rb') as stream:
        for raw in stream:
            if not raw.endswith(b'\n'):
                incomplete_tail = True
                break
            item = json.loads(raw)
            kind = item['event']
            if kind == 'input':
                inputs[item['id']] = item['data']
            elif kind == 'shader':
                shaders[item['id']] = item
            elif kind == 'frame':
                failed += item['failures']
                rejected += item['rejected']
            elif kind == 'sample':
                data = inputs[item['input']]
                states = dict(data['states'])
                world = data['perspective'] and data.get('primaryTarget') is True and states.get(7, 0) != 0
                scope = 'primary_perspective_depth' if world else 'other_or_unclassified'
                # D3DRS_LIGHTING does not select whether a bound VS computes lighting.
                vs = data['vertexShader']
                path_name = 'vertex_shader' if vs else ('ffp_lighting' if states[137] and not data.get('channels', 0) & 8 else 'ffp_unlit')
                counts[(scope, path_name)] += 1
                references[item['input']] += 1
                frames.add(item['frame'])
    ambient = []
    for identity, data in inputs.items():
        vs = data['vertexShader']
        if not vs:
            continue
        states = dict(data['states'])
        for color in vs['colors']:
            if color['name'] != 'AmbientCol':
                continue
            packed = states[139]
            ambient_state = [(packed >> shift & 255) / 255 for shift in (16, 8, 0)]
            material = data['material']['ambient'][:3]
            actual = color['rgba'][:3]
            finite = all(value is not None for value in material + actual)
            product = [a * b for a, b in zip(ambient_state, material)] if finite else None
            delta = max(abs(a - b) for a, b in zip(actual, product)) if finite else None
            ambient.append(dict(input=identity, sampledDraws=references[identity],
                                materialAmbient=material, deviceAmbient=ambient_state,
                                shaderAmbient=actual, product=product, maxDifference=delta,
                                tolerance=3e-6, matchesProduct=delta is not None and delta <= 3e-6))
    shader_verification = []
    if verify_shaders:
        from analyze_shader_audit import D3dx, digest
        sdk = D3dx()
        selected = {'AmbientCol', 'MatDiffuse', 'MatSpecular', 'ConstColor',
                    'LightAmbientColorDir0', 'LightDiffuseColorDir0'}
        for identity, shader in shaders.items():
            if not shader.get('dumped') or not isinstance(identity, int) or not 1 <= identity <= 256:
                raise ValueError('Run has no complete bounded shader dump')
            bytecode = (run / f'materials.jsonl.shader-{identity:04}.bin').read_bytes()
            if len(bytecode) != shader['bytes']:
                raise ValueError('Shader dump size differs from audit')
            assembly, _ = sdk.disassemble(bytecode)
            expected = sorted((name, int(register)) for name, register in
                              re.findall(r'^//\s+(\w+)\s+c(\d+)\s+\d+\s*$', assembly, re.M)
                              if name in selected)
            for data in inputs.values():
                vs = data['vertexShader']
                if vs and vs['id'] == identity:
                    actual = sorted((color['name'], color['register']) for color in vs['colors'])
                    if actual != expected:
                        raise ValueError(f'Shader {identity} reflection mismatch: {actual} vs {expected}')
            shader_verification.append(dict(id=identity, sha256=digest(bytecode),
                                            sdkColorRegisters=expected, status='PASS'))
    return dict(schema=1, scope='Sampled original draw inputs; primary perspective/depth is a pass filter, not proven native camera identity',
                sampledFrames=len(frames), firstFrame=min(frames, default=None), lastFrame=max(frames, default=None),
                sampledDraws=sum(references.values()), uniqueInputs=len(inputs), uniqueShaders=len(shaders),
                failures=failed, rejected=rejected, incompleteTail=incomplete_tail,
                groups=[dict(scope=scope, path=path_name, draws=count) for (scope, path_name), count in sorted(counts.items())],
                shaderAmbientComparisons=ambient, shaderVerification=shader_verification,
                limits=['No GPU or server constant-value comparison',
                        'No inference that FFP vertex color is baked lighting',
                        'Ambient product agreement is an observation, not a replacement game algorithm',
                        'Shader reflection reports selected named color constants, not all shader behavior',
                        'Inputs deduplicated by observed state; IDs do not identify game materials or objects',
                        'Only sampled frames are covered; unreadable inputs and capacity rejections are counted'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--verify-shaders', action='store_true', help='Compare saved live CTAB registers with installed Microsoft D3DX')
    args = parser.parse_args()
    report = analyze(args.run, args.verify_shaders)
    (args.run / 'material-analysis.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
