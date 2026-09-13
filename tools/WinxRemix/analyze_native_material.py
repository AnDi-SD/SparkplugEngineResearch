"""Summarize closed native material runs and explicit remaining D3D guards."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from analyze_native_draw import records
from analyze_native_mesh_source import exited

REASONS = ('scope', 'mode', 'material', 'pass', 'layer', 'texture', 'override',
           'coordinates', 'arguments', 'mapping', 'device')


def analyze(run):
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    exited(launch['pid'])
    path = run / 'native-material-source.jsonl'
    before = path.stat()
    events, totals, reasons, histogram, operations, samplers, sources = (Counter() for _ in range(7))
    init = None
    first = last = None
    switches = []
    materials, textures = set(), set()
    for row in records(path):
        kind = row['event']; events[kind] += 1
        if kind == 'init':
            if init is not None:
                raise ValueError('Duplicate native material initialization')
            init = row
        elif kind == 'frame':
            frame = row['frame']
            if last is not None and frame <= last:
                raise ValueError('Non-monotonic material frame records')
            if first is None:
                first = frame
            last = frame
            if len(row['rejected']) != len(REASONS):
                raise ValueError('Unknown rejection schema')
            if row['matched'] + sum(row['rejected']) != row['attempts'] or row['used'] > row['matched']:
                raise ValueError('Inconsistent material source counters')
            totals.update({k: row[k] for k in ('attempts', 'matched', 'used', 'mismatches')})
            reasons.update(dict(zip(REASONS, row['rejected'])))
            histogram[row['used']] += 1
        elif kind == 'submit':
            materials.add(row['material']); textures.add(row['texture'])
            operations[f"{row['rgbOperation']}/{row['alphaOperation']}"] += 1
            samplers[','.join(map(str, row['sampler']))] += 1
            sources[row['source']] += 1
        elif kind == 'source_switch':
            switches.append(row)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('Material log changed during analysis')
    if not init or not init['enabled']:
        raise ValueError('Native material source was not enabled')
    return dict(schema=1, run=str(run), pidExited=True, proxySha256=launch['proxySha256'],
                sourceLogSha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=before.st_size,
                logLimitReached=before.st_size >= init['maxLogBytes'], frameRange=[first, last],
                events=dict(events), totals=dict(totals), rejected=dict(reasons),
                nativeFrameHistogram=dict(histogram), sampledNativeOperations=dict(operations),
                sampledNativeSamplers=dict(samplers), sampledSources=dict(sources),
                sampledDistinctMaterialAddresses=len(materials), sampledDistinctTextureAddresses=len(textures),
                switches=switches,
                scope='Recorded ordinary single StdLayer mode2 UV0 untransformed draws only. Native operations/sampler use shared mapping; unlit coefficients use existing adapter policy. Arguments, render/blend states, resource bytes and identity validation retain D3D. Addresses are observations, not durable instance identities or whole-scene coverage.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.run)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    print(json.dumps(result))
