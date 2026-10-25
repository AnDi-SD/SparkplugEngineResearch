"""Summarize a closed independent Remix submission run, including rejected work."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from analyze_native_draw import records
from analyze_native_mesh_source import exited

COUNTERS = ('groups', 'instances', 'rejected', 'cameraRejected', 'queueRejected',
            'apiFailures', 'unexpectedDraws', 'suppressedDraws', 'selectionTransitions')


def summarize(rows):
    init = None
    events, totals, histogram, sampled = (Counter() for _ in range(4))
    first = last = failure_frame = None
    disabled = False
    modes = Counter()
    for row in rows:
        kind = row['event']
        events[kind] += 1
        if kind == 'init':
            if init is not None or row.get('schema') != 1 or row.get('enabled') is not True:
                raise ValueError('Unexpected independent submission initialization')
            init = row
            continue
        if init is None:
            raise ValueError('Data before initialization')
        frame = row['frame']
        if type(frame) is not int or frame < 0:
            raise ValueError('Invalid frame')
        if kind == 'frame':
            if last is not None and frame <= last:
                raise ValueError('Non-monotonic completed frames')
            if any(type(row[k]) is not int or row[k] < 0 for k in COUNTERS):
                raise ValueError('Invalid submission counters')
            if type(row['comparison']) is not bool or type(row['disabledAfterFailure']) is not bool:
                raise ValueError('Invalid submission mode')
            if disabled and not row['disabledAfterFailure']:
                raise ValueError('Failure latch unexpectedly reset')
            if row['suppressedDraws']:
                raise ValueError('This consumer must always forward native draws')
            if row['comparison'] and (row['groups'] or row['instances']):
                raise ValueError('Independent work in comparison mode')
            if row['instances'] and not row['groups']:
                raise ValueError('Instances without a claimed group')
            if first is None:
                first = frame
            last = frame
            if row['disabledAfterFailure'] and not disabled:
                failure_frame = frame
            disabled = row['disabledAfterFailure']
            totals.update({k: row[k] for k in COUNTERS})
            modes['comparison' if row['comparison'] else 'direct'] += 1
            histogram[(row['comparison'], row['groups'], row['instances'], row['rejected'],
                       row['cameraRejected'], row['queueRejected'], disabled)] += 1
        elif kind == 'instance':
            if row['originallySelected'] is not False or type(row['result']) is not int:
                raise ValueError('Unexpected independent instance source/result')
            for key in ('scope', 'scene', 'support', 'model', 'mesh', 'cpuGeneration',
                        'textureGeneration', 'textureContent'):
                if type(row[key]) is not int or row[key] <= 0:
                    raise ValueError('Missing instance provenance')
            if type(row['occurrence']) is not int or row['occurrence'] < 0:
                raise ValueError('Invalid repeated Model occurrence')
            sampled[(row['scene'], row['result'])] += 1
        elif kind == 'unexpected_selection':
            if type(row['scope']) is not int or row['scope'] <= 0:
                raise ValueError('Invalid selection transition')
        else:
            raise ValueError(f'Unknown independent submission event: {kind}')
    if init is None or first is None:
        raise ValueError('Missing initialization/completed frames')
    return dict(schema=1, frameRange=[first, last], events=dict(events), totals=dict(totals),
                modes=dict(modes), firstDisabledFrame=failure_frame, disabledAfterFailure=disabled,
                frameHistogram=[dict(comparison=k[0], groups=k[1], instances=k[2], rejected=k[3],
                                     cameraRejected=k[4], queueRejected=k[5], disabled=k[6], frames=v)
                                for k, v in sorted(histogram.items())],
                sampledResults=[dict(scene=k[0], result=k[1], samples=v) for k, v in sorted(sampled.items())],
                scope='Completed-frame client API result counts for complete supported supports absent from original selection. Groups count claimed groups, including partial API failures; instances count successful client DrawInstance results. The installed x86 bridge returns SUCCESS after enqueueing without a per-instance server acknowledgement: this log does not prove renderer API acceptance or GPU completion. Sampled instance rows are not an exhaustive scene inventory. Original native draws are always forwarded. A selection transition or partial API failure cannot retract previously enqueued instances; the failure latch prevents further direct submission until restart. Zero reported failures is not proof of complete game coverage or image correctness.')


def analyze(run):
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    exited(launch['pid'])
    path = run / 'independent-scene-submit.jsonl'
    before = path.stat()
    if before.st_size > 17 * 1024 * 1024:
        raise ValueError('Unexpected direct log size')
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
