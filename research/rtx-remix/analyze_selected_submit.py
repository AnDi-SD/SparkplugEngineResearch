"""Closed-run accounting for before-Prepare native packets committed at indexed draws."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from analyze_native_draw import records
from analyze_native_mesh_source import exited

COUNTERS = ('attempts', 'calls', 'instances', 'rejected', 'apiFailures',
            'reentries', 'postCommitFaults')


def summarize(rows):
    init = None
    first = last = first_disabled = None
    disabled = False
    events, totals, histogram, samples = (Counter() for _ in range(4))
    for row in rows:
        kind = row['event']
        events[kind] += 1
        if kind == 'init':
            if init is not None or row.get('schema') != 1 or row.get('enabled') is not True:
                raise ValueError('Invalid selected consumer initialization')
            init = row
            continue
        if init is None:
            raise ValueError('Missing initialization')
        frame = row['frame']
        if type(frame) is not int or frame < 0:
            raise ValueError('Invalid frame')
        if kind == 'frame':
            if last is not None and frame <= last:
                raise ValueError('Non-monotonic completed frames')
            if any(type(row[k]) is not int or row[k] < 0 for k in COUNTERS):
                raise ValueError('Invalid selected counters')
            if row['attempts'] != row['calls'] + row['rejected'] or row['calls'] != row['instances'] + row['apiFailures']:
                raise ValueError('Lost or double-counted selected attempt')
            if row['postCommitFaults'] > row['calls']:
                raise ValueError('Post-commit fault without a committed call')
            if type(row['disabledAfterFailure']) is not bool or (disabled and not row['disabledAfterFailure']):
                raise ValueError('Invalid or cleared failure latch')
            if (row['apiFailures'] or row['postCommitFaults']) and not row['disabledAfterFailure']:
                raise ValueError('Committed failure did not disable selected submission')
            if first is None:
                first = frame
            last = frame
            if row['disabledAfterFailure'] and not disabled:
                first_disabled = frame
            disabled = row['disabledAfterFailure']
            totals.update({k: row[k] for k in COUNTERS})
            histogram[tuple(row[k] for k in COUNTERS) + (disabled,)] += 1
        elif kind == 'instance':
            for key in ('scope', 'scene', 'support', 'model', 'modelCall', 'submission',
                        'cpuGeneration', 'textureGeneration', 'textureContent'):
                if type(row[key]) is not int or row[key] <= 0:
                    raise ValueError('Missing selected instance provenance')
            if type(row['draw']) is not int or row['draw'] < 0 or type(row['result']) is not int:
                raise ValueError('Invalid selected draw/result')
            samples[(row['scene'], row['result'])] += 1
        else:
            raise ValueError(f'Unknown selected event: {kind}')
    if init is None or first is None:
        raise ValueError('Missing initialization/completed frames')
    return dict(schema=1, frameRange=[first, last], events=dict(events), totals=dict(totals),
                firstDisabledFrame=first_disabled, disabledAfterFailure=disabled,
                frameHistogram=[dict(zip(COUNTERS + ('disabledAfterFailure', 'frames'), key + (count,)))
                                for key, count in sorted(histogram.items())],
                sampledResults=[dict(scene=key[0], result=key[1], samples=count)
                                for key, count in sorted(samples.items())],
                scope='Originally selected ordinary Model inputs come from the current before-Prepare packet; original producers still run. Commit occurs at the qualified indexed draw, so scheduling still depends on D3D. Instances count successful x86 client enqueue results without per-instance server acknowledgement, not renderer acceptance or GPU completion. API errors/post-commit faults retain ownership of the intercepted draw and disable future selected submission. Pre-commit rejection leaves the previous path available. Sampled provenance is not an exhaustive scene inventory.')


def analyze(run):
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    exited(launch['pid'])
    path = run / 'selected-scene-submit.jsonl'
    before = path.stat()
    if before.st_size > 17 * 1024 * 1024:
        raise ValueError('Unexpected selected log size')
    result = summarize(records(path))
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('Log changed during analysis')
    result.update(run=str(run), pidExited=True, proxySha256=launch['proxySha256'],
                  sourceLogSha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=before.st_size,
                  logLimitReached=before.st_size >= 16 * 1024 * 1024)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = analyze(args.run)
    with args.output.open('x', encoding='utf-8') as target:
        json.dump(result, target, indent=2)
    print(json.dumps(result))
