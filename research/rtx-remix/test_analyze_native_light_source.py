"""Owned JSON/IO boundary tests; no game, original instructions or GPU."""
from copy import deepcopy
import argparse
import json
import os
from pathlib import Path
from unittest.mock import patch
import analyze_native_light_source as analyzer


def run(directory):
    directory.mkdir(parents=True, exist_ok=False)
    init = dict(event='init', schema=1, enabled=True, submit=True, pid=123,
                maxLogBytes=16777216)
    samples = [dict(event='compare', frame=10, scene=65536, object=70000 + t,
                    type=t, consumedMask=mask, differenceMask=2 if t == 1 else 0,
                    source='spLight_fields_world_shared_PC4B53C0')
               for t, mask in [(0, 0x7000F), (1, 0xE8E00F), (2, 0x3FFE00F)]]
    frame = dict(event='frame', frame=10, attempts=5, matched=2, used=2,
                 mismatches=1, dirty=1, invalid=1, comparison=False)
    comparison = dict(frame, frame=11, attempts=1, matched=1, used=0,
                      mismatches=0, dirty=0, invalid=0, comparison=True)
    valid = [init] + samples + [frame, comparison]
    checks = 0

    def reject(action):
        nonlocal checks
        try:
            action()
        except ValueError:
            checks += 1
        else:
            raise AssertionError('Invalid evidence accepted')

    result = analyzer.summarize(valid)
    assert result['totals'] == dict(attempts=6, matched=3, used=2, mismatches=1, dirty=1, invalid=1)
    assert result['sampledTypes'] == {0: 1, 1: 1, 2: 1} and result['incompleteSampleSuffix'] is None
    checks += 1
    result = analyzer.summarize([dict(init, submit=False)] + samples + [dict(frame, used=0)])
    assert result['totals']['used'] == 0
    checks += 1
    result = analyzer.summarize(valid + [dict(samples[0], frame=12)])
    assert result['incompleteSampleSuffix']['frame'] == 12 and result['totals']['matched'] == 3
    checks += 1
    invalids = [valid[1:], [init], [init, init] + valid[1:],
                valid + [comparison], valid + [dict(samples[0], frame=11)],
                [dict(init, schema=True)] + valid[1:],
                [dict(init, submit=1)] + valid[1:],
                [dict(init, maxLogBytes=4096)] + valid[1:],
                [dict(init, pid=False)] + valid[1:]]
    for key, value in [('attempts', 4), ('used', 3), ('matched', True), ('dirty', -1),
                       ('invalid', 0x100000000), ('comparison', 0)]:
        invalids.append([init] + samples + [dict(frame, **{key: value})])
    invalids += [[init] + samples + [dict(frame, comparison=True)],
                 [dict(init, submit=False)] + samples + [frame],
                 [init, frame],  # mismatch sample cannot silently disappear
                 [init] + samples + [dict(frame, matched=1, attempts=4, used=1)]]
    for key, value in [('type', 3), ('type', True), ('consumedMask', 15),
                       ('differenceMask', 0x80000000), ('source', 'unverified'),
                       ('scene', 0), ('object', -1), ('frame', 9)]:
        invalids.append([init, samples[0], dict(samples[1], **{key: value})] + samples[2:] + [frame])
    invalids += [[init, samples[0], dict(samples[1], frame=11)],
                 valid + [dict(comparison, event='unknown', frame=12)]]
    for rows in invalids:
        reject(lambda rows=deepcopy(rows): analyzer.summarize(rows))
    # Diagnostics preserve the failed invariant and original totals, not an allow-list.
    result = analyzer.summarize([init] + samples + [dict(frame, comparison=True)], diagnostics=True)
    assert result['status'] == 'FAIL' and result['strictViolations'] == [dict(
        frame=10, constraint='comparison_implies_used_zero', comparison=True, used=2)]
    assert result['totals']['used'] == 2
    checks += 1
    reject(lambda: analyzer.summarize([dict(init, submit=False)] + samples + [frame], diagnostics=True))
    reject(lambda: analyzer.summarize([init] + samples + [dict(frame, attempts=4)], diagnostics=True))
    reject(lambda: analyzer.decode(b'{"event":"init","event":"frame"}\n', lines=True))
    reject(lambda: analyzer.decode(b'{"event":"init"}', lines=True))
    oversized = directory / 'oversized.bin'
    oversized.write_bytes(b'x' * 17)
    reject(lambda: analyzer.read_closed(oversized, 16))

    launch, log = directory / 'launch.json', directory / 'native-light-source.jsonl'
    def write(pid=123, log_pid=123):
        launch.write_text(json.dumps(dict(pid=pid, nativeLightSource=True, proxySha256='0' * 64)))
        rows = [dict(init, pid=log_pid)] + samples + [frame, comparison]
        log.write_text(''.join(json.dumps(row) + '\n' for row in rows))
    write()
    # Only process-exit is mocked here; file parsing/identity checks are real.
    with patch.object(analyzer, 'exited') as process_check:
        result = analyzer.analyze(directory)
        assert result['immutableRead'] and process_check.call_count == 2
        checks += 1
        write(log_pid=124)
        reject(lambda: analyzer.analyze(directory))
    write(pid=os.getpid(), log_pid=os.getpid())
    reject(lambda: analyzer.analyze(directory))  # actual currently running CPU test PID
    write()
    calls = 0
    def mutate_on_final_check(pid):
        nonlocal calls
        calls += 1
        if calls == 2:
            with log.open('a') as stream:
                stream.write('\n')
    with patch.object(analyzer, 'exited', mutate_on_final_check):
        reject(lambda: analyzer.analyze(directory))
    return dict(status='PASS', checks=checks, gpu=False, nativeCode=False,
                fileBoundaryFixture=str(directory), gameExecuted=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture-dir', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.fixture_dir)))
