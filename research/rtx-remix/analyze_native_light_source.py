"""Strict closed-log accounting for the consumed native PC light-field source."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
from analyze_native_mesh_source import exited

MAX_BYTES = 16 * 1024 * 1024
COUNTERS = ('attempts', 'matched', 'used', 'mismatches', 'dirty', 'invalid')
# Schema-1 masks of the adapter's consumed payload words, not game arithmetic.
MASKS = {0: 0x7000F, 1: 0xE8E00F, 2: 0x3FFE00F}
SOURCE = 'spLight_fields_world_shared_PC4B53C0'


def require(value, message):
    if not value:
        raise ValueError(message)


def uint(value, name, positive=False):
    require(type(value) is int and (1 if positive else 0) <= value <= 0xFFFFFFFF,
            'Invalid unsigned ' + name)
    return value


def summarize(rows, diagnostics=False):
    init = None
    first = last = previous = None
    pending_frame = None
    pending = Counter()
    events, totals, histogram, types, differences = (Counter() for _ in range(5))
    objects = set()
    violations = []
    for row in rows:
        require(type(row) is dict, 'Record must be an object')
        kind = row.get('event')
        require(kind in ('init', 'compare', 'frame'), 'Unknown native-light event')
        events[kind] += 1
        if kind == 'init':
            require(init is None and sum(events.values()) == 1, 'Duplicate or late initialization')
            require(type(row.get('schema')) is int and row['schema'] == 1 and
                    row.get('enabled') is True and type(row.get('submit')) is bool,
                    'Invalid native-light initialization')
            uint(row.get('pid'), 'pid', True)
            require(type(row.get('maxLogBytes')) is int and row['maxLogBytes'] == MAX_BYTES,
                    'Unknown native-light log bound')
            init = row
            continue
        require(init is not None, 'Missing initialization')
        frame = uint(row.get('frame'), 'frame')
        require(previous is None or frame >= previous, 'Decreasing event frame')
        previous = frame
        if kind == 'compare':
            require(last is None or frame > last, 'Compare after completed frame')
            require(pending_frame is None or frame == pending_frame, 'Unclosed earlier sample frame')
            pending_frame = frame
            light_type = uint(row.get('type'), 'type')
            mask = uint(row.get('consumedMask'), 'consumedMask')
            diff = uint(row.get('differenceMask'), 'differenceMask')
            require(light_type in MASKS and mask == MASKS[light_type], 'Unknown type/consumed mask')
            require(not diff & ~mask, 'Difference outside consumed fields')
            require(row.get('source') == SOURCE, 'Unknown native producer source')
            scene = uint(row.get('scene'), 'scene', True)
            obj = uint(row.get('object'), 'object', True)
            objects.add((scene, obj))
            types[light_type] += 1
            differences[diff] += 1
            pending['mismatches' if diff else 'matched'] += 1
            continue
        require(last is None or frame > last, 'Duplicate/non-monotonic completed frame')
        values = {key: uint(row.get(key), key) for key in COUNTERS}
        require(type(row.get('comparison')) is bool, 'Invalid comparison flag')
        require(values['attempts'] == sum(values[key] for key in ('matched', 'mismatches', 'dirty', 'invalid')),
                'Lost or double-counted native-light attempt')
        require(values['used'] <= values['matched'], 'Use without exact match')
        require(not values['used'] or init['submit'], 'Use while native submission disabled')
        if values['used'] and row['comparison']:
            require(diagnostics, 'Use while comparison enabled at frame ' + str(frame))
            violations.append(dict(frame=frame, constraint='comparison_implies_used_zero',
                                   comparison=True, used=values['used']))
        require(pending_frame is None or pending_frame == frame, 'Missing completed sample frame')
        require(pending['matched'] <= values['matched'] and pending['mismatches'] == values['mismatches'],
                'Sample/counter disagreement; every mismatch is logged')
        pending.clear()
        pending_frame = None
        first = frame if first is None else first
        last = frame
        totals.update(values)
        histogram[tuple(values[key] for key in COUNTERS) + (row['comparison'],)] += 1
    require(init is not None and first is not None, 'Missing initialization/completed frames')
    return dict(schema=1, status='FAIL' if violations else 'PASS', strictViolations=violations,
                frameRange=[first, last], init=init, events=dict(events), totals=dict(totals),
                frameHistogram=[dict(zip(COUNTERS + ('comparison', 'frames'), key + (count,)))
                                for key, count in sorted(histogram.items())],
                sampledTypes=dict(types), sampledDifferenceMasks=dict(differences),
                sampledDistinctSceneObjectPairs=len(objects),
                incompleteSampleSuffix=dict(frame=pending_frame, counts=dict(pending)) if pending_frame is not None else None,
                scope='Completed native-light attempt counters and sampled consumed-word comparisons only. '
                      'Used means computed consumed fields selected for the existing light converter, not a renderer API '
                      'call, GPU completion, all payload bytes or physical-light correctness. Sampled addresses are not '
                      'durable identities. Missing frame rows do not imply zero attempts or absent scene lights.')


def identity(stat):
    return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns


def read_closed(path, limit):
    before = identity(path.stat())
    require(before[2] <= limit, 'Oversized log/metadata')
    with path.open('rb') as stream:
        require(identity(os.fstat(stream.fileno())) == before, 'File replaced before read')
        data = stream.read(limit + 1)
        require(len(data) <= limit and identity(os.fstat(stream.fileno())) == before, 'File changed during read')
    require(identity(path.stat()) == before, 'File replaced after read')
    return data, before


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key')
        result[key] = value
    return result


def decode(data, lines=False):
    if lines:
        require(data.endswith(b'\n'), 'Incomplete last JSONL row')
        return [json.loads(line, object_pairs_hook=unique_object) for line in data.decode('utf-8-sig').splitlines()]
    return json.loads(data.decode('utf-8-sig'), object_pairs_hook=unique_object)


def analyze(run, diagnostics=False):
    launch_path, path = run / 'launch.json', run / 'native-light-source.jsonl'
    metadata, launch_identity = read_closed(launch_path, 1024 * 1024)
    launch = decode(metadata)
    pid = uint(launch.get('pid'), 'launch PID', True)
    exited(pid)
    require(launch.get('nativeLightSource', True) is True, 'Native light source disabled in launch')
    data, log_identity = read_closed(path, MAX_BYTES + 4096)  # one bounded fprintf may cross the cap
    result = summarize(decode(data, lines=True), diagnostics)
    require(result['init']['pid'] == pid, 'Launch/log PID mismatch')
    exited(pid)
    require(identity(path.stat()) == log_identity and identity(launch_path.stat()) == launch_identity,
            'Evidence changed after analysis')
    result.update(run=str(run), pidExited=True, immutableRead=True, proxySha256=launch.get('proxySha256'),
                  sourceLogSha256=hashlib.sha256(data).hexdigest().upper(), bytes=len(data),
                  launchSha256=hashlib.sha256(metadata).hexdigest().upper(), logLimitReached=len(data) >= MAX_BYTES)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--diagnostics', action='store_true',
                        help='Write comparison-label violations with FAIL status and exit 2; never waive them')
    args = parser.parse_args()
    result = analyze(args.run, args.diagnostics)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    print(json.dumps({key: result[key] for key in ('status', 'strictViolations', 'totals', 'frameRange', 'logLimitReached')}))
    raise SystemExit(2 if result['status'] == 'FAIL' else 0)
