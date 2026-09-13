"""Summarize a closed native world-update witness log, without running the game.

Phase serials include EndFrame and standalone-root invalidations. They are not
manager-call counts. Root calls include descendants; accepted queries are not
independent geometry submissions or complete descendant-update coverage.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from analyze_native_mesh_source import exited

LOG_LIMIT = 16 * 1024 * 1024
LINE_LIMIT = 4096
HISTORY_LIMIT = 64
COUNTERS = ('begun', 'completed', 'rootCalls', 'recorded', 'published',
            'aborted', 'overflowed', 'queries', 'accepted')


def number(row, key):
    value = row[key]
    if type(value) is not int or value < 0:
        raise ValueError(f'Invalid nonnegative integer: {key}')
    return value


def summarize(path):
    """Pure bounded file analysis; analyze() additionally requires an exited PID."""
    before = path.stat()
    if before.st_size > LOG_LIMIT + LINE_LIMIT:
        raise ValueError('World-update log exceeds its bounded observer budget')
    digest = hashlib.sha256()
    totals, events, histogram = Counter(), Counter(), Counter()
    init = first = last = first_serial = last_serial = None
    histories = {k: [] for k in ('aborted', 'overflowed', 'uncompleted', 'queryRejected')}
    history_counts = Counter()
    queried_frames = all_accepted_frames = gaps = 0
    max_published = max_recorded = max_begun = 0
    with path.open('rb') as stream:
        while line := stream.readline(LINE_LIMIT + 1):
            if len(line) > LINE_LIMIT or not line.endswith(b'\n'):
                raise ValueError('Oversized record or incomplete world-update log tail')
            digest.update(line)
            row = json.loads(line.decode('utf-8-sig'))
            kind = row['event']
            events[kind] += 1
            if kind == 'init':
                if init is not None or events['frame']:
                    raise ValueError('Duplicate or late world-update initialization')
                if row.get('schema') != 1 or row.get('capacity') != 64:
                    raise ValueError('Unsupported world-update schema/capacity')
                if type(row.get('enabled')) is not bool or type(row.get('imageVerified')) is not bool:
                    raise ValueError('Invalid initialization flags')
                if row['enabled'] and not row['imageVerified']:
                    raise ValueError('Enabled hooks without verified image')
                init = row
                continue
            if kind != 'frame' or init is None:
                raise ValueError('Unexpected record or frame before initialization')
            if not init['enabled']:
                raise ValueError('Frame counters emitted by disabled hooks')
            frame, serial = number(row, 'frame'), number(row, 'sequence')
            values = {k: number(row, k) for k in COUNTERS}
            if last is not None and (frame <= last or serial <= last_serial):
                raise ValueError('Non-monotonic frame or invalidation serial')
            if first is None:
                first, first_serial = frame, serial
            else:
                gaps += frame - last - 1
            last, last_serial = frame, serial
            if (values['completed'] > values['begun'] or values['accepted'] > values['queries'] or
                    values['published'] > values['recorded'] or values['recorded'] > values['rootCalls'] or
                    values['published'] > init['capacity'] * values['completed']):
                raise ValueError('Inconsistent world-update counters')
            totals.update(values)
            histogram[values['published']] += 1
            queried_frames += values['queries'] > 0
            all_accepted_frames += values['queries'] > 0 and values['accepted'] == values['queries']
            max_published = max(max_published, values['published'])
            max_recorded = max(max_recorded, values['recorded'])
            max_begun = max(max_begun, values['begun'])
            failures = dict(aborted=values['aborted'], overflowed=values['overflowed'],
                            uncompleted=values['begun'] - values['completed'],
                            queryRejected=values['queries'] - values['accepted'])
            for reason, count in failures.items():
                if count:
                    history_counts[reason] += 1
                    if len(histories[reason]) < HISTORY_LIMIT:
                        histories[reason].append(dict(frame=frame, sequence=serial, count=count))
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('World-update log changed during analysis')
    if init is None:
        raise ValueError('Missing world-update initialization')
    return dict(schema=1, init=init, bytes=before.st_size, sourceLogSha256=digest.hexdigest().upper(),
                logLimitBytes=LOG_LIMIT, logLimitReached=before.st_size >= LOG_LIMIT,
                frameRange=[first, last], omittedFrameNumbers=gaps, events=dict(events), totals=dict(totals),
                managerCalls=totals['begun'], completedManagerBatches=totals['completed'],
                uncompletedManagerBatches=totals['begun'] - totals['completed'],
                queryAcceptance=dict(queries=totals['queries'], accepted=totals['accepted'],
                                     rejected=totals['queries'] - totals['accepted'],
                                     ratio=totals['accepted'] / totals['queries'] if totals['queries'] else None,
                                     queriedFrames=queried_frames, allQueriesAcceptedFrames=all_accepted_frames),
                capacity=dict(perBatch=init['capacity'], overflowEvents=totals['overflowed'],
                              maxPublishedPerLoggedFrame=max_published, maxRecordedPerLoggedFrame=max_recorded,
                              maxManagerBeginsPerLoggedFrame=max_begun),
                publishedPerFrameHistogram=dict(sorted(histogram.items())),
                sequence=dict(firstObserved=first_serial, lastObserved=last_serial,
                              isManagerCount=False, meaning='Atomic phase invalidation serial; also EndFrame and standalone world producers'),
                histories={k: dict(frames=history_counts[k], first=histories[k],
                                   truncated=history_counts[k] > len(histories[k])) for k in histories},
                limits=['Counters are render-thread observer events, not all-thread native function totals.',
                        'completed means qualified normal manager completion, not a native success return.',
                        'rootCalls includes descendant/standalone plain Node calls; published counts tokens, not unique scenes.',
                        'No per-scene/SystemRoot pointer is present in this aggregate log.',
                        'Clean aborted/overflow counters do not prove absence of every rejected root or other game/renderer error.',
                        'GetWitness acceptance is not independent mesh submission or proof of all descendant updates.',
                        'Final unpresented activity is not flushed into these frame counters.'])


def analyze(run):
    launch_path = run / 'launch.json'
    if launch_path.stat().st_size > 1024 * 1024:
        raise ValueError('Oversized launch metadata')
    with launch_path.open('rb') as stream:
        launch_bytes = stream.read(1024 * 1024 + 1)
    if len(launch_bytes) > 1024 * 1024:
        raise ValueError('Launch metadata grew beyond budget')
    launch = json.loads(launch_bytes.decode('utf-8-sig'))
    exited(launch['pid'])
    result = summarize(run / 'native-update-source.jsonl')
    exited(launch['pid'])
    if launch_path.stat().st_size != len(launch_bytes):
        raise ValueError('Launch metadata size changed during analysis')
    with launch_path.open('rb') as stream:
        after_launch = stream.read(1024 * 1024 + 1)
    if after_launch != launch_bytes:
        raise ValueError('Launch metadata changed during analysis')
    result.update(run=str(run), pid=launch['pid'], pidExited=True, started=launch.get('started'),
                  startLevel=launch.get('startLevel'), proxySha256=launch['proxySha256'],
                  executableSha256=launch['executableSha256'],
                  launchSha256=hashlib.sha256(launch_bytes).hexdigest().upper())
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.run)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
    print(json.dumps(result))
