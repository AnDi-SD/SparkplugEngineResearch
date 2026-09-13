"""Strict closed sampled-vertex accounting, not whole-game weighted geometry coverage."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
from analyze_native_skin_source import (MAX_BYTES, MAX_ROW, decode, exited, identity,
                                       read_closed, require, uint)

SOURCE_SCOPE = ('sampled qualified Skin calls; exact CPU-source/upload and shared-layout comparison; '
                'no weighted API submission; unit-sum tolerance1e-5')
COUNTERS = ('attempts', 'matched', 'rejected')
SUMMARY_UINT = ('vertices', 'influences', 'maxIndex', 'negativeWeightVertices', 'nonUnitSumVertices')


def finite(value, name, nonnegative=False):
    try:
        valid = type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        valid = False
    require(valid and (not nonnegative or value >= 0), 'Invalid finite ' + name)
    return value


def inspect_summary(row):
    values = {key: uint(row.get(key), key) for key in SUMMARY_UINT}
    low, high = (finite(row.get(key), key) for key in ('minWeight', 'maxWeight'))
    error = finite(row.get('maxSumError'), 'maxSumError', True)
    vertices = values['vertices']
    if not vertices:
        require(not any(values.values()) and low == high == error == 0, 'Nonzero empty summary')
    else:
        require(vertices <= 65536 and 1 <= values['influences'] <= 4, 'Unsupported vertex/influence bound')
        require(values['maxIndex'] < row['boneCount'], 'Palette index outside bone count')
        require(values['negativeWeightVertices'] <= vertices and values['nonUnitSumVertices'] <= vertices,
                'Diagnostic vertex count exceeds inspected vertices')
        require(low <= high, 'Inverted weight extrema')
        require((values['negativeWeightVertices'] > 0) == (low < 0), 'Negative diagnostic/extremum disagreement')
        require((values['nonUnitSumVertices'] > 0) == (error > 1e-5), 'Unit-sum diagnostic/error disagreement')
    return values


def summarize(rows):
    init = None
    first = last = previous = pending_frame = None
    call = submission = 0
    events, totals, sample_totals, pending, rejections, histogram, cohorts = (Counter() for _ in range(7))
    diagnostic = Counter()
    min_weight = max_weight = None
    max_error = 0
    failure_count = 0
    failure_history = []
    for row in rows:
        require(type(row) is dict, 'Record must be an object')
        event = row.get('event')
        require(event in ('init', 'sample', 'frame'), 'Unknown native-skin-vertices event')
        events[event] += 1
        if event == 'init':
            require(init is None and sum(events.values()) == 1, 'Duplicate or late initialization')
            require(type(row.get('schema')) is int and row['schema'] == 1 and row.get('enabled') is True,
                    'Invalid initialization schema/enable')
            uint(row.get('pid'), 'pid', positive=True)
            require(type(row.get('maxLogBytes')) is int and row['maxLogBytes'] == MAX_BYTES and
                    row.get('scope') == SOURCE_SCOPE, 'Unknown producer scope/bound')
            init = row
            continue
        require(init is not None, 'Missing initialization')
        frame = uint(row.get('frame'), 'frame')
        require(previous is None or frame >= previous, 'Decreasing event frame')
        previous = frame
        if event == 'sample':
            require(last is None or frame > last, 'Sample after completed frame')
            require(pending_frame is None or frame == pending_frame, 'Unclosed earlier sample frame')
            pending_frame = frame
            for name in ('skin', 'mesh', 'scene'):
                uint(row.get(name), name, positive=True)
            current_call = uint(row.get('modelCall'), 'modelCall', 64, True)
            current_submit = uint(row.get('submission'), 'submission', 64, True)
            require(current_call >= call and current_submit > submission, 'Non-monotonic sampled sequence')
            call, submission = current_call, current_submit
            accepted = row.get('accepted')
            require(type(accepted) is bool, 'Invalid acceptance flag')
            reason = uint(row.get('rejection'), 'rejection')
            generation = uint(row.get('generation'), 'generation', 64)
            require((accepted and reason == 0 and generation > 0) or
                    (not accepted and 1 <= reason <= 7 and generation == 0), 'Acceptance/reason/generation disagreement')
            bones = uint(row.get('boneCount'), 'boneCount', positive=True)
            require(bones <= 256, 'Unsupported bone count')
            values = inspect_summary(row)
            require(not accepted or values['vertices'] > 0, 'Accepted empty summary')
            require(reason not in range(1, 6) or values['vertices'] == 0, 'Premature rejection with completed inspection')
            require(reason != 6 or values['vertices'] > 0, 'Final fence rejection without completed inspection')
            # Exception rejection7 may occur either before or after Inspect.
            for target in (pending, sample_totals):
                target['attempts'] += 1
                target['matched' if accepted else 'rejected'] += 1
            if not accepted:
                rejections[reason] += 1
                failure_count += 1
                if len(failure_history) < 64:
                    failure_history.append(row)
            if accepted:
                for key in ('vertices', 'negativeWeightVertices', 'nonUnitSumVertices'):
                    diagnostic[key] += values[key]
                min_weight = row['minWeight'] if min_weight is None else min(min_weight, row['minWeight'])
                max_weight = row['maxWeight'] if max_weight is None else max(max_weight, row['maxWeight'])
                max_error = max(max_error, row['maxSumError'])
                cohorts[(row['influences'], bones)] += 1
            continue
        require(last is None or frame > last, 'Duplicate/non-monotonic completed frame')
        counts = {key: uint(row.get(key), key) for key in COUNTERS}
        require(counts['attempts'] == counts['matched'] + counts['rejected'], 'Lost or double-counted attempt')
        require(pending_frame is None or pending_frame == frame, 'Missing completed sample frame')
        require(all(counts[key] == pending[key] for key in COUNTERS), 'Frame counters differ from actual samples')
        totals.update(counts)
        histogram[tuple(counts[key] for key in COUNTERS)] += 1
        first = frame if first is None else first
        last = frame
        pending.clear()
        pending_frame = None
    require(init is not None, 'Missing initialization')
    return dict(schema=1, status='PASS', init=init, events=dict(events), frameRange=[first, last],
                totals={key: totals[key] for key in COUNTERS},
                sampledTotals={key: sample_totals[key] for key in COUNTERS},
                rejections={str(key): rejections[key] for key in range(1, 8)},
                frameHistogram=[dict(zip(COUNTERS + ('frames',), key + (n,))) for key, n in sorted(histogram.items())],
                acceptedSampleDiagnostics=dict(**diagnostic, minWeight=min_weight, maxWeight=max_weight, maxSumError=max_error),
                sampledCohorts=[dict(influences=k[0], boneCount=k[1], samples=n) for k, n in sorted(cohorts.items())],
                failureSamples=failure_count, failureHistory=failure_history,
                failureHistoryTruncated=failure_count > len(failure_history),
                incompleteSampleSuffix=dict(frame=pending_frame, counts=dict(pending)) if pending_frame is not None else None,
                sampledSequenceEnd=dict(modelCall=call, submission=submission),
                scope='Only sampled already-qualified Skin calls. Completed frame totals equal emitted sample records; '
                      'all-sample diagnostics may include the explicitly incomplete suffix. Vertices are unique '
                      'referenced vertices per sample, repeatedly counted across samples. Negative weights and nonunit '
                      'sums are diagnostics, not validation failures or evidence of bad native arithmetic. Accepted '
                      'means CPU source/upload/layout and operation fences passed, not skin deformation, renderer '
                      'admission, GPU completion, all Skins or durable native lifetime coverage.')


def validate(data):
    require(0 < len(data) <= MAX_BYTES + MAX_ROW and data.endswith(b'\n'), 'Unexpected size or partial last row')
    lines = data.splitlines(keepends=True)
    offset = 0
    for line in lines:
        require(0 < len(line) <= MAX_ROW and offset < MAX_BYTES, 'Invalid row/cap boundary')
        offset += len(line)
    result = summarize(decode(line) for line in lines)
    result.update(bytes=len(data), logLimitReached=len(data) >= MAX_BYTES, logCapMarker=False)
    return result


def analyze(run):
    import hashlib
    run = Path(run)
    launch_path, log = run / 'launch.json', run / 'native-skin-vertices.jsonl'
    metadata, launch_identity = read_closed(launch_path, 1024 * 1024)
    launch = decode(metadata)
    require(type(launch) is dict, 'Invalid launch metadata')
    pid = uint(launch.get('pid'), 'launch PID', positive=True)
    exited(pid)
    require(launch.get('nativeSkinVertices', True) is True, 'Vertex observer disabled in launch')
    data, log_identity = read_closed(log, MAX_BYTES + MAX_ROW)
    result = validate(data)
    require(result['init']['pid'] == pid, 'Launch/log PID mismatch')
    exited(result['init']['pid'])
    require(identity(log.stat()) == log_identity and identity(launch_path.stat()) == launch_identity,
            'Evidence changed after analysis')
    result.update(run=str(run), pidExited=True, immutableRead=True, proxySha256=launch.get('proxySha256'),
                  sourceLogSha256=hashlib.sha256(data).hexdigest().upper(),
                  launchSha256=hashlib.sha256(metadata).hexdigest().upper())
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.run)
    with args.output.open('x', encoding='utf-8', newline='\n') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
    print(json.dumps({key: result[key] for key in ('status', 'totals', 'sampledTotals', 'acceptedSampleDiagnostics',
                                                 'logLimitReached', 'incompleteSampleSuffix')}))
