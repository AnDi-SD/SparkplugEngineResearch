"""Summarize a closed before-prepare scene-input comparison, without claiming export."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from analyze_native_draw import records
from analyze_native_mesh_source import exited

REASONS = ('phase', 'registry', 'support', 'hierarchy', 'world', 'model', 'callbacks',
           'material', 'pass', 'controllers', 'override', 'texture', 'mapping', 'capacity', 'stale', 'missing')
COUNTERS = ('scans', 'candidates', 'selected', 'dirty', 'compared', 'matched', 'worldDifferences', 'materialDifferences')
RESOURCE_COUNTERS = ('resourceCandidates', 'resourceCompared', 'resourceMatched', 'resourceDifferences',
                     'resourceRejected', 'uploadDifferences')
DRAW_COUNTERS = ('drawCandidates', 'drawCompared', 'drawMatched', 'drawDifferences', 'drawRejected')
TEXTURE_COUNTERS = ('textureCandidates', 'textureCompared', 'textureMatched', 'textureDifferences', 'textureRejected')
TEXTURE_SAMPLE = ('textureReady', 'textureMatch', 'textureGeneration', 'textureContent')


def analyze(run):
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    exited(launch['pid'])
    path = run / 'independent-scene-source.jsonl'
    before = path.stat()
    if before.st_size > 17 * 1024 * 1024:
        raise ValueError('Unexpected log size')
    events, totals, rejections, histogram, kinds, outcomes, device_differences = (Counter() for _ in range(7))
    scans, updates, scenes = set(), set(), set()
    first = last = init = resource_fields = draw_fields = texture_fields = None
    resource_histogram, resource_outcomes = Counter(), Counter()
    draw_histogram, draw_outcomes = Counter(), Counter()
    texture_histogram, texture_outcomes = Counter(), Counter()
    max_world = max_inverse = 0
    for row in records(path):
        kind = row['event']; events[kind] += 1
        if kind == 'init':
            if init is not None or row.get('schema') != 1 or row.get('submit') is not False:
                raise ValueError('Unexpected observer initialization')
            init = row
        elif kind == 'frame':
            frame = row['frame']
            if type(frame) is not int or frame < 0 or (last is not None and frame <= last):
                raise ValueError('Non-monotonic frame')
            present = all(k in row for k in RESOURCE_COUNTERS)
            if any(k in row for k in RESOURCE_COUNTERS) != present:
                raise ValueError('Partial resource counters')
            if resource_fields is None:
                resource_fields = present
            elif resource_fields != present:
                raise ValueError('Resource counter schema changed within run')
            counters = COUNTERS + (RESOURCE_COUNTERS if present else ())
            draw_present = all(k in row for k in DRAW_COUNTERS)
            if any(k in row for k in DRAW_COUNTERS) != draw_present:
                raise ValueError('Partial draw counters')
            if draw_fields is None:
                draw_fields = draw_present
            elif draw_fields != draw_present:
                raise ValueError('Draw counter schema changed within run')
            if draw_present:
                counters += DRAW_COUNTERS
            texture_present = all(k in row for k in TEXTURE_COUNTERS)
            if any(k in row for k in TEXTURE_COUNTERS) != texture_present:
                raise ValueError('Partial texture counters')
            if texture_fields is None:
                texture_fields = texture_present
            elif texture_fields != texture_present:
                raise ValueError('Texture counter schema changed within run')
            if texture_present:
                counters += TEXTURE_COUNTERS
            values = [row[k] for k in counters] + row['rejected']
            if any(type(v) is not int or v < 0 for v in values) or len(row['rejected']) != len(REASONS):
                raise ValueError('Unexpected counters')
            if (row['matched'] > row['compared'] or row['selected'] > row['candidates'] or
                    row['dirty'] > row['candidates'] or row['worldDifferences'] > row['compared'] or
                    row['materialDifferences'] > row['compared']):
                raise ValueError('Inconsistent observer counts')
            if first is None:
                first = frame
            last = frame
            if present:
                if (row['resourceCandidates'] + row['resourceRejected'] != row['candidates'] or
                        row['resourceCompared'] > row['compared'] or
                        row['resourceMatched'] + row['resourceDifferences'] != row['resourceCompared']):
                    raise ValueError('Inconsistent resource counters')
                resource_histogram[tuple(row[k] for k in RESOURCE_COUNTERS[:5])] += 1
            if draw_present:
                if (row['drawCandidates'] + row['drawRejected'] != row['candidates'] or
                        row['drawCompared'] > row['compared'] or
                        row['drawMatched'] + row['drawDifferences'] != row['drawCompared']):
                    raise ValueError('Inconsistent draw counters')
                draw_histogram[tuple(row[k] for k in DRAW_COUNTERS)] += 1
            if texture_present:
                if (row['textureCandidates'] + row['textureRejected'] != row['candidates'] or
                        row['textureCompared'] > row['compared'] or
                        row['textureMatched'] + row['textureDifferences'] != row['textureCompared']):
                    raise ValueError('Inconsistent texture counters')
                texture_histogram[tuple(row[k] for k in TEXTURE_COUNTERS)] += 1
            totals.update({k: row[k] for k in counters})
            rejections.update(dict(zip(REASONS, row['rejected'])))
            histogram[tuple(row[k] for k in ('candidates', 'selected', 'dirty', 'compared', 'matched'))] += 1
        elif kind == 'scan':
            scans.add(row['scope']); updates.add(row['update']); scenes.add(row['scene'])
        elif kind == 'compare':
            if type(row['world']) is not bool or type(row['material']) is not bool:
                raise ValueError('Nonboolean comparison')
            kinds[row['kind']] += 1
            outcomes[(row['world'], row['material'], row['computedWorld'], row['originallySelected'],
                      row.get('alphaInputBeforeAdapter'))] += 1
            max_world = max(max_world, row['maxWorldError'])
            max_inverse = max(max_inverse, row.get('maxInverseError', 0))
            if row.get('deviceDifferences'):
                device_differences[tuple(row['firstDeviceDifference'])] += 1
            if 'resourcesReady' in row:
                if type(row['resourcesReady']) is not bool or type(row['resourcesMatch']) is not bool:
                    raise ValueError('Nonboolean resource comparison')
                resource_outcomes[(row['resourcesReady'], row['resourcesMatch'])] += 1
            if 'drawReady' in row:
                if type(row['drawReady']) is not bool or type(row['drawReasons']) is not int or row['drawReasons'] < 0:
                    raise ValueError('Unexpected draw comparison')
                draw_outcomes[(row['drawReady'], row['drawReasons'])] += 1
            texture_sample = all(k in row for k in TEXTURE_SAMPLE)
            if any(k in row for k in TEXTURE_SAMPLE) != texture_sample:
                raise ValueError('Partial texture comparison')
            if texture_sample:
                ready, match, generation, content = (row[k] for k in TEXTURE_SAMPLE)
                if (type(ready) is not bool or type(match) is not bool or
                        type(generation) is not int or type(content) is not int or
                        generation < 0 or content < 0 or (match and not ready) or
                        (ready and (generation == 0 or content == 0))):
                    raise ValueError('Unexpected texture comparison')
                texture_outcomes[(ready, match, generation, content)] += 1
        else:
            raise ValueError(f'Unknown observer event: {kind}')
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('Log changed during analysis')
    if not init or not init['enabled']:
        raise ValueError('Independent input observer was not enabled')
    return dict(schema=1, run=str(run), pidExited=True, proxySha256=launch['proxySha256'],
                sourceLogSha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=before.st_size,
                logLimitReached=before.st_size >= init['maxLogBytes'], frameRange=[first, last],
                events=dict(events), totals=dict(totals), rejected=dict(rejections),
                frameHistogram=[dict(candidates=k[0], selected=k[1], dirty=k[2], compared=k[3], matched=k[4], frames=v)
                                for k, v in sorted(histogram.items())], sampledOwnerKinds=dict(kinds),
                sampledOutcomes=[dict(world=k[0], material=k[1], computedWorld=k[2], originallySelected=k[3],
                                     alphaInputBeforeAdapter=k[4], samples=v) for k, v in outcomes.items()],
                sampledFirstDeviceDifferences=[dict(state=k[0], expected=k[1], actual=k[2], samples=v)
                                               for k, v in device_differences.items()],
                sampledMaxWorldError=max_world, sampledMaxInverseError=max_inverse,
                resourceObservation=bool(resource_fields),
                resourceFrameHistogram=[dict(zip(RESOURCE_COUNTERS[:5], k), frames=v)
                                        for k, v in sorted(resource_histogram.items())],
                sampledResourceOutcomes=[dict(ready=k[0], matched=k[1], samples=v)
                                         for k, v in resource_outcomes.items()],
                drawObservation=bool(draw_fields),
                drawFrameHistogram=[dict(zip(DRAW_COUNTERS, k), frames=v) for k, v in sorted(draw_histogram.items())],
                sampledDrawOutcomes=[dict(ready=k[0], reasons=k[1], samples=v) for k, v in draw_outcomes.items()],
                textureObservation=bool(texture_fields),
                textureFrameHistogram=[dict(zip(TEXTURE_COUNTERS, k), frames=v)
                                       for k, v in sorted(texture_histogram.items())],
                sampledTextureOutcomes=[dict(ready=k[0], matched=k[1], generation=k[2], contentGeneration=k[3], samples=v)
                                        for k, v in texture_outcomes.items()],
                sampledScopes=len(scans), sampledUpdateEpochs=len(updates), sampledScenes=sorted(scenes),
                scope='Fresh unique (support,Model) inputs per scene scope before original Prepare; comparisons count actual repeated successful API submits. World/material observer with optional current-resource and texture lifecycle/content stamps, not independent geometry export, immutable mesh proof, durable instance identity, or full scene coverage. Texture match means the native COM pointer and borrowed generation/content token remained current, not a texture byte comparison. OriginallySelected refers to the unextended support selection. Known adapter alpha adjustment is credited only from its live draw-scope saved input. Frame aggregates and sampled comparisons have different denominators.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.run)
    with args.output.open('x', encoding='utf-8') as target:
        json.dump(result, target, indent=2)
    print(json.dumps(result))
