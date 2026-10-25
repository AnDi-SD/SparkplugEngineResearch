"""Strict closed-log accounting for the native skin palette observer; no geometry credit."""
import argparse
from collections import Counter
import ctypes
from ctypes import wintypes
import hashlib
import json
import math
import os
from pathlib import Path

MAX_BYTES = 16 * 1024 * 1024
MAX_ROW = 4096
MAX_BONES = 256
REASONS = ('scope', 'callsite', 'scene', 'registry', 'support', 'skin', 'mesh',
           'palette', 'bone', 'nonfinite', 'changed')
COUNTERS = ('calls', 'callFailures', 'withoutMesh', 'attempts', 'matched',
            'mismatches', 'bones', 'bitDifferences')
ERRORS = ('maxAbsoluteError', 'maxRelativeError')
SOURCE_SCOPE = ('own Skin mesh call; current plain Node cached world PRS; bounded tolerant palette '
                'observer, not weighted geometry submission or durable lifetime')


def require(value, message):
    if not value:
        raise ValueError(message)


def uint(value, name, bits=32, positive=False):
    require(type(value) is int and (1 if positive else 0) <= value < 1 << bits,
            'Invalid unsigned ' + name)
    return value


def error_values(row):
    values = {}
    for key in ERRORS:
        value = row.get(key)
        try:
            finite = type(value) in (int, float) and math.isfinite(value)
        except OverflowError:
            finite = False
        require(finite and value >= 0,
                'Invalid finite error ' + key)
        values[key] = value
    return values


def summarize(rows):
    """Validate owned schema, not the underlying recovered arithmetic."""
    init = None
    first = last = previous = pending_frame = None
    model_call = submission = 0
    events, totals, rejects, histogram, samples, weights = (Counter() for _ in range(6))
    pending = Counter()
    maxima = dict.fromkeys(ERRORS, 0)
    pending_max = dict.fromkeys(ERRORS, 0)
    sampled_max = dict.fromkeys(ERRORS, 0)
    objects = set()
    failure_history = []
    failures = 0
    for row in rows:
        require(type(row) is dict, 'Record must be an object')
        kind = row.get('event')
        require(kind in ('init', 'frame', 'compare'), 'Unknown native-skin event')
        events[kind] += 1
        if kind == 'init':
            require(init is None and sum(events.values()) == 1, 'Duplicate or late initialization')
            require(type(row.get('schema')) is int and row['schema'] == 1,
                    'Unknown native-skin schema')
            require(type(row.get('enabled')) is bool and type(row.get('imageVerified')) is bool,
                    'Invalid initialization flags')
            require(not row['enabled'] or row['imageVerified'], 'Enabled without verified image')
            uint(row.get('pid'), 'pid', positive=True)
            require(type(row.get('maxLogBytes')) is int and row['maxLogBytes'] == MAX_BYTES and
                    type(row.get('maxBones')) is int and row['maxBones'] == MAX_BONES,
                    'Unknown native-skin bound')
            require(type(row.get('absoluteTolerance')) in (int, float) and
                    type(row.get('relativeTolerance')) in (int, float) and
                    row['absoluteTolerance'] == 0.0001 and row['relativeTolerance'] == 0.00001,
                    'Unknown native-skin tolerances')
            require(row.get('rejectionNames') == list(REASONS), 'Unknown rejection ordering')
            require(row.get('scope') == SOURCE_SCOPE, 'Unknown initialization scope')
            init = row
            continue
        require(init is not None, 'Missing initialization')
        frame = uint(row.get('frame'), 'frame')
        require(previous is None or frame >= previous, 'Decreasing event frame')
        previous = frame
        values = error_values(row)
        bits = uint(row.get('bitDifferences'), 'bitDifferences')
        require(bits != 0 or not any(values.values()), 'Numeric error without differing bits')
        if kind == 'compare':
            require(init['enabled'], 'Compare while observer disabled')
            require(last is None or frame > last, 'Compare after completed frame')
            require(pending_frame is None or frame == pending_frame, 'Unclosed earlier sample frame')
            pending_frame = frame
            for name in ('scene', 'skin', 'support', 'camera', 'mesh', 'renderer', 'palette'):
                uint(row.get(name), name, positive=True)
            bones = uint(row.get('boneCount'), 'boneCount', positive=True)
            require(bones <= MAX_BONES and bits <= bones * 16, 'Invalid comparison bone/bit count')
            for name in ('weightHint', 'meshWeights'):
                uint(row.get(name), name)
            current_call = uint(row.get('modelCall'), 'modelCall', 64, True)
            current_submit = uint(row.get('submission'), 'submission', 64, True)
            require(current_call >= model_call and current_submit > submission,
                    'Non-monotonic sampled call/submission sequence')
            model_call, submission = current_call, current_submit
            tolerance = row.get('withinTolerance')
            require(type(tolerance) is bool, 'Invalid tolerance result')
            require(tolerance or (bits > 0 and values['maxAbsoluteError'] > 0),
                    'Tolerance failure without numeric difference')
            key = 'matched' if tolerance else 'mismatches'
            for target in (samples, pending):
                target[key] += 1
                target['bones'] += bones
                target['bitDifferences'] += bits
            for name in ERRORS:
                pending_max[name] = max(pending_max[name], values[name])
                sampled_max[name] = max(sampled_max[name], values[name])
            weights[(row['weightHint'], row['meshWeights'], bones)] += 1
            objects.add((row['scene'], row['skin'], row['support']))
            continue
        require(last is None or frame > last, 'Duplicate/non-monotonic completed frame')
        counts = {key: uint(row.get(key), key) for key in COUNTERS}
        rejected = row.get('rejected')
        require(type(rejected) is list and len(rejected) == len(REASONS), 'Invalid rejection vector')
        rejected = {key: uint(value, key) for key, value in zip(REASONS, rejected)}
        require(counts['attempts'] == counts['matched'] + counts['mismatches'] + sum(rejected.values()),
                'Lost or double-counted skin attempt')
        require(counts['callFailures'] <= counts['calls'] and counts['withoutMesh'] <= counts['calls'],
                'Failure/without-mesh count exceeds original calls')
        compared = counts['matched'] + counts['mismatches']
        require(compared <= counts['bones'] <= MAX_BONES * compared and bits <= counts['bones'] * 16,
                'Invalid frame bone/bit count')
        require(not counts['mismatches'] or (bits > 0 and values['maxAbsoluteError'] > 0),
                'Frame tolerance failure without numeric difference')
        require(init['enabled'] or (not any(counts.values()) and not any(rejected.values())),
                'Work while observer disabled')
        require(pending_frame is None or pending_frame == frame, 'Missing completed sample frame')
        require(all(pending[key] <= counts[key] for key in ('matched', 'bones', 'bitDifferences')) and
                pending['mismatches'] == counts['mismatches'], 'Sample/counter disagreement')
        require(all(pending_max[key] <= values[key] for key in ERRORS), 'Sample error exceeds frame maximum')
        if counts['callFailures'] or counts['withoutMesh'] or counts['mismatches'] or any(rejected.values()):
            failures += 1
            if len(failure_history) < 64:
                failure_history.append(dict(frame=frame, **counts, rejected=rejected, **values))
        pending.clear()
        pending_frame = None
        pending_max = dict.fromkeys(ERRORS, 0)
        first = frame if first is None else first
        last = frame
        totals.update(counts)
        rejects.update(rejected)
        for key in ERRORS:
            maxima[key] = max(maxima[key], values[key])
        histogram[tuple(counts[key] for key in COUNTERS) + tuple(rejected[key] for key in REASONS)] += 1
    require(init is not None, 'Missing initialization')
    return dict(schema=1, status='PASS', init=init, events=dict(events), frameRange=[first, last],
                totals={key: totals[key] for key in COUNTERS}, rejected={key: rejects[key] for key in REASONS},
                **maxima, completedFrames=events['frame'], observationEnabled=init['enabled'],
                frameHistogram=[dict(zip(COUNTERS, key[:len(COUNTERS)]),
                                     rejected=dict(zip(REASONS, key[len(COUNTERS):])), frames=count)
                                for key, count in sorted(histogram.items())],
                sampledTotals=dict(samples), sampledMaxErrors=sampled_max,
                sampledWeights=[dict(weightHint=k[0], meshWeights=k[1], boneCount=k[2], samples=n)
                                for k, n in sorted(weights.items())],
                sampledDistinctSceneSkinSupportTuples=len(objects),
                sampledSequenceEnd=dict(modelCall=model_call, submission=submission),
                failureFrameCount=failures, failureFrameHistory=failure_history,
                failureFrameHistoryTruncated=failures > len(failure_history),
                incompleteSampleSuffix=dict(frame=pending_frame, counts=dict(pending), **pending_max)
                if pending_frame is not None else None,
                scope='Completed frame counters and sampled native palette comparisons only. Matched means every '
                      'compared float passed the per-word tolerance, not necessarily bit equality. Bit differences '
                      'can pass tolerance (including signed zero). Calls, modelCall and submission are separate '
                      'observer scopes/sequences, not submitted geometry counts. Addresses are not durable identities. '
                      'No weighted geometry, renderer acceptance or GPU completion credit. Missing frames and a '
                      'terminal sample suffix are not counted as zero work or complete coverage.')


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key')
        result[key] = value
    return result


def decode(data):
    return json.loads(data.decode('utf-8-sig'), object_pairs_hook=unique_object,
                      parse_constant=lambda value: require(False, 'Non-finite JSON constant ' + value))


def validate(data):
    require(0 < len(data) <= MAX_BYTES + MAX_ROW, 'Unexpected log size')
    require(data.endswith(b'\n'), 'Incomplete last JSONL row')
    lines = data.splitlines(keepends=True)
    require(all(0 < len(line) <= MAX_ROW for line in lines), 'Empty or oversized JSONL row')
    offset = 0
    for line in lines:
        require(offset < MAX_BYTES, 'Record begins after producer log cap')
        offset += len(line)
    result = summarize(decode(line) for line in lines)
    result.update(bytes=len(data), logLimitReached=len(data) >= MAX_BYTES,
                  logCapMarker=False, sourceLogSha256=hashlib.sha256(data).hexdigest().upper())
    return result


def identity(stat):
    return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns


def read_closed(path, limit):
    before = identity(path.stat())
    require(before[2] <= limit, 'Oversized log/metadata')
    with path.open('rb') as stream:
        require(identity(os.fstat(stream.fileno())) == before, 'File replaced before read')
        data = stream.read(limit + 1)
        require(len(data) == before[2] and identity(os.fstat(stream.fileno())) == before,
                'File changed during read')
    require(identity(path.stat()) == before, 'File replaced after read')
    return data, before


def exited(pid):
    uint(pid, 'recorded PID', positive=True)
    require(os.name == 'nt', 'Recorded process closure must be verified on Windows')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.WaitForSingleObject.restype = wintypes.DWORD
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x100000, False, pid)
    if not handle:
        error = ctypes.get_last_error()
        if error == 87:
            return
        raise ctypes.WinError(error)
    try:
        state = kernel.WaitForSingleObject(handle, 0)
        if state == 0xFFFFFFFF:
            raise ctypes.WinError(ctypes.get_last_error())
        require(state == 0, 'Recorded PID is still running (including PID reuse)')
    finally:
        kernel.CloseHandle(handle)


def analyze(run):
    run = Path(run)
    launch_path, path = run / 'launch.json', run / 'native-skin-source.jsonl'
    metadata, launch_identity = read_closed(launch_path, 1024 * 1024)
    launch = decode(metadata)
    require(type(launch) is dict, 'Invalid launch metadata')
    pid = uint(launch.get('pid'), 'launch PID', positive=True)
    exited(pid)
    require(launch.get('nativeSkinSource', True) is True, 'Native skin source disabled in launch')
    data, log_identity = read_closed(path, MAX_BYTES + MAX_ROW)
    result = validate(data)
    require(result['init']['pid'] == pid, 'Launch/log PID mismatch')
    exited(result['init']['pid'])
    require(identity(path.stat()) == log_identity and identity(launch_path.stat()) == launch_identity,
            'Evidence changed after analysis')
    result.update(run=str(run), pidExited=True, immutableRead=True, proxySha256=launch.get('proxySha256'),
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
    print(json.dumps({key: result[key] for key in ('status', 'totals', 'rejected', 'frameRange',
                                                 'logLimitReached', 'incompleteSampleSuffix')}))
