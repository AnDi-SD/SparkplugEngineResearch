"""Summarize closed native camera runs without equating API return to GPU proof."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from analyze_native_draw import records
from analyze_native_mesh_source import exited


def analyze(run):
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    exited(launch['pid'])
    path = run / 'native-camera-source.jsonl'
    before = path.stat()
    events, totals, before_draw, after_draw, reasons, epochs = (Counter() for _ in range(6))
    init = None
    first = last = None
    matched_frames = submitted_frames = repeated_submissions = 0
    switches = []
    packets = []
    for row in records(path):
        kind = row['event']; events[kind] += 1
        if kind == 'init':
            if init is not None:
                raise ValueError('Duplicate camera initialization')
            init = row
        elif kind == 'frame':
            frame = row['frame']
            if last is not None and frame <= last:
                raise ValueError('Non-monotonic camera frame records')
            if first is None:
                first = frame
            last = frame
            totals.update({k: row[k] for k in ('applies', 'foreignApplies', 'rejected', 'matched',
                                              'submitted', 'failures', 'stateMismatches', 'lateUpdates', 'missedFirstDraws')})
            matched_frames += bool(row['matched'])
            submitted_frames += bool(row['submitted'])
            repeated_submissions += row['submitted'] > 1
            epochs[row['deviceEpoch']] += 1
        elif kind == 'camera':
            if len(row['viewBits']) != 16 or len(row['projectionBits']) != 16:
                raise ValueError('Truncated camera matrix packet')
            before_draw[row['beforeDraw']] += 1; after_draw[row['applyAfterDraw']] += 1
            packets.append(row)
        elif kind == 'first_window_missed':
            reasons[row['reason']] += 1
        elif kind == 'source_switch':
            switches.append(row)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('Camera log changed during analysis')
    if not init or not init['enabled']:
        raise ValueError('Native camera source was not installed')
    return dict(schema=1, run=str(run), pidExited=True, proxySha256=launch['proxySha256'],
                components={k: launch.get(k) for k in ('remixClientSha256', 'remixServerSha256', 'remixRendererSha256')},
                sourceLogSha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=before.st_size,
                logLimitReached=before.st_size >= init['maxLogBytes'], frameRange=[first, last],
                events=dict(events), totals=dict(totals), matchedFrames=matched_frames,
                submittedFrames=submitted_frames, framesWithMultipleSubmissions=repeated_submissions,
                sampledBeforeDrawHistogram=dict(before_draw), sampledApplyAfterDrawHistogram=dict(after_draw),
                sampledMissedReasons=dict(reasons), deviceEpochFrames=dict(epochs), switches=switches,
                sampledPackets=packets,
                scope='Recorded adapter frames and sampled camera packets only. API success is the renderer API return through the bridge; GPU camera values/cut history are not directly instrumented. v1 observer lateUpdates did not compare after observation; v2+ uses selected snapshot.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.run)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    print(json.dumps({k: v for k, v in result.items() if k != 'sampledPackets'}))
