"""Validate bounded Skin material/texture sidecars and their actual draw links.

This is capture validation, not a shader classifier or Remix material approval.
"""
from pathlib import Path
import argparse, hashlib, json, struct


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def texture(path):
    size = path.stat().st_size
    if not 28 <= size <= 8 * 1024 * 1024:
        raise ValueError('SKT1 byte bound')
    data = path.read_bytes()
    magic, version, total, width, height, levels, format = struct.unpack_from('<7I', data)
    if (magic, version, total) != (0x31544B53, 1, len(data)):
        raise ValueError('SKT1 header/extent')
    if not (0 < width <= 4096 and 0 < height <= 4096 and 0 < levels <= 16 and format in (21, 22)):
        raise ValueError('SKT1 supported dimensions/format')
    mips, offset, w, h = [], 28, width, height
    for level in range(levels):
        if offset + 16 > len(data):
            raise ValueError('Truncated mip header')
        mw, mh, stride, count = struct.unpack_from('<4I', data, offset)
        offset += 16
        if (mw, mh, stride, count) != (w, h, w * 4, w * h * 4) or offset + count > len(data):
            raise ValueError('Mip extent or pitch')
        pixels = memoryview(data)[offset:offset + count]
        alpha = pixels[3::4]
        mips.append({'level': level, 'width': w, 'height': h, 'bytes': count,
            'sha256': digest(pixels), 'alphaMinimum': min(alpha) if format == 21 else 255,
            'alphaMaximum': max(alpha) if format == 21 else 255,
            'nonOpaquePixels': sum(a != 255 for a in alpha) if format == 21 else 0})
        offset += count
        if w == h == 1 and level + 1 != levels:
            raise ValueError('Duplicate terminal mip')
        w, h = max(1, w // 2), max(1, h // 2)
    if offset != len(data):
        raise ValueError('Trailing texture bytes')
    return {'file': path.name, 'sha256': digest(data), 'bytes': len(data),
        'width': width, 'height': height, 'format': format, 'levels': levels, 'mips': mips}


def events(path, maximum):
    if path.stat().st_size > maximum:
        raise ValueError('Journal byte bound')
    return [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines() if line]


def analyze(run):
    folder = run / 'skin-packets'
    rows = events(folder / 'packets.jsonl', 1024 * 1024)
    packets = {r['file']: r for r in rows if r['event'] == 'packet'}
    draws = [r for r in rows if r['event'] == 'draw_result']
    if not 0 < len(packets) <= 64 or not 0 < len(draws) <= 128:
        raise ValueError('Packet/draw count bound')
    shaders = {(r['frame'], r['draw']): r for r in events(run / 'shader-semantics.jsonl', 16 * 1024 * 1024) if r['event'] == 'draw'}
    textures, records, missing = {}, [], []
    for draw in draws:
        if not draw.get('materialWritten'):
            missing.append({'file': draw['file'], 'frame': draw['frame'], 'draw': draw['draw']})
            continue
        packet = packets[draw['file']]
        name = Path(draw['file'])
        if name.name != draw['file'] or name.suffix != '.skp':
            raise ValueError('Packet path')
        path = folder / f'{name.stem}-draw-{draw["draw"]:06}.material.json'
        if path.stat().st_size > 16384:
            raise ValueError('Material byte bound')
        material = json.loads(path.read_text(encoding='utf-8'))
        if not (material['schema'] == 1 and material['frame'] == draw['frame'] and material['draw'] == draw['draw']
                and material['packet'] == int(name.stem.split('-')[1]) and material['shader'] == draw['shader']
                and material['stateStable'] and material['textureStable'] and not material['materialQualified']):
            raise ValueError('Material identity/scope')
        shader = shaders[(draw['frame'], draw['draw'])]
        if not (draw['originalHr'] == 0 and draw['boundInputMatches'] and draw['constantsWritten'] and draw['reason'] == 0
                and shader['bytecodeEqual'] and shader['shader'] == draw['shader']
                and shader['skinCall'] == packet['modelCall'] == draw['modelCall']
                and shader['meshSubmission'] == packet['submission'] == draw['submission']):
            raise ValueError('Original draw/selection link')
        ordinal = material['textureFile']
        if not isinstance(ordinal, int) or not 1 <= ordinal <= 64:
            raise ValueError('Texture ordinal')
        if ordinal not in textures:
            textures[ordinal] = texture(folder / f'texture-{ordinal:04}.skt')
        if sum(t['bytes'] for t in textures.values()) > 32 * 1024 * 1024:
            raise ValueError('Aggregate texture byte bound')
        states, stage, sampler = dict(material['states']), dict(material['stages'][0]), dict(material['samplers'][0])
        records.append({'packet': draw['file'], 'frame': draw['frame'], 'draw': draw['draw'],
            'vertices': packet['vertices'], 'key': shader['key'], 'textureFile': ordinal,
            'nativeMaterial': material['nativeMaterial'], 'textureGeneration': material['textureGeneration'],
            'textureContentGeneration': material['textureContentGeneration'], 'sidecarSha256': digest(path.read_bytes()),
            'pixelShader': material['pixelShader'], 'rgbOperation': stage[1], 'alphaOperation': stage[4],
            'rgbArguments': [stage[2], stage[3]], 'alphaArguments': [stage[5], stage[6]],
            'coordinates': stage[11], 'transformFlags': stage[24],
            'alphaTest': [states[15], states[25], states[24]], 'blend': [states[27], states[19], states[20], states[171]],
            'cull': states[22], 'sampler': sampler})
    return {'status': 'PASS' if not missing else 'INCOMPLETE', 'packets': len(packets), 'draws': len(draws),
        'materialDraws': len(records), 'vertices': sum(r['vertices'] for r in records),
        'textures': len(textures), 'textureBytes': sum(t['bytes'] for t in textures.values()),
        'materialQualified': False, 'scope': __doc__, 'missing': missing, 'records': records,
        'textureRecords': list(textures.values())}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError('Choose a fresh report path')
    result = analyze(args.run.resolve())
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('records', 'textureRecords', 'missing')}))
    raise SystemExit(result['status'] != 'PASS')
