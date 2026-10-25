"""Check observed child exit codes, release timing, ownership and source identity."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    run = args.run.resolve()
    output = run / 'result.json'
    assert not output.exists(), 'Preserve previous qualification'
    def read(name):
        return json.loads((run / name).read_text(encoding='utf-8-sig'))
    source, launch, execution = map(read, ('source.json', 'launch.json', 'execution.json'))
    for name, digest in source['files'].items():
        assert hashlib.sha256((run / name).read_bytes()).hexdigest() == digest, name
    binary_sha = hashlib.sha256((run / 'test_shutdown.exe').read_bytes()).hexdigest()
    assert binary_sha.upper() == launch['sha256']
    assert execution['pid'] == launch['pid'] and execution['exitCode'] == 0
    assert not any(execution[k] for k in ('memoryLimited', 'timedOut', 'failure', 'remaining'))
    assert not any(row['forcedByWrapper'] for row in execution['children'])
    rows = [json.loads(line) for line in (run / 'fixture.jsonl').read_text().splitlines()]
    if source.get('kind') == 'RemoteHandle':
        assert rows[-1] == dict(event='complete', cases=2)
        cases = [row for row in rows if row.get('event') == 'case']
        balances = [row for row in rows if row.get('event') == 'handle_balance']
        children = [row for row in rows if row.get('event') == 'child']
        warmup = [row for row in rows if row.get('event') == 'warmup']
        assert len(cases) == len(balances) == len(children) == 2 and len(warmup) == 1
        assert warmup[0]['exitCode'] == 0 and not any('error' in row for row in rows)
        for index, case in enumerate(cases):
            assert case['index'] == balances[index]['index'] == children[index]['index'] == index
            assert case['profile'] == children[index]['profile'] == ('baseline' if index == 0 else 'candidate')
            assert case['passed'] and case['remoteObjectIsParent']
            assert case['parentMarkerOpen'] == case['expectedMarkerOpen'] == bool(index)
            assert case['handlesBefore'] == case['handlesAfter'] == balances[index]['before'] == balances[index]['after']
            assert case['childExitCode'] == balances[index]['childExitCode'] == 0
            assert 0 < case['remoteAllocations'] <= 4096 and case['milliseconds'] <= 2000
        report = dict(status='PASS', kind='RemoteHandle', cases=cases, warmup=warmup[0], source=source, execution=execution,
                      scope='Two own process handle tables. An event and remote parent-process handle share only their numeric value. Candidate preserves the parent event; each case returns to its initial handle count. No callback or full bridge qualification.')
        with output.open('x', encoding='utf-8') as stream:
            json.dump(report, stream, indent=2)
        print(json.dumps(dict(status='PASS', kind='RemoteHandle', cases=2, milliseconds=execution['milliseconds'], remaining=execution['remaining'])))
        return
    assert rows[-1] == dict(event='complete', cases=8)
    cases = [row for row in rows if row.get('event') == 'case']
    children = [row for row in rows if row.get('event') == 'child']
    assert len(cases) == len(children) == 8 and not any('error' in row for row in rows)
    names = ['fast', 'slow-clean', 'reported-error', 'hung']
    for index, case in enumerate(cases):
        profile = 'baseline' if index < 4 else 'candidate'
        name = names[index % 4]
        expected_exit = {'fast': 0, 'reported-error': 23, 'hung': 1}.get(name, 1 if index < 4 else 0)
        bounds = (0, 2000) if name in ('fast', 'reported-error') else (2750, 4500) if index < 4 else (3500, 6500) if name == 'slow-clean' else (9750, 12500)
        assert case['index'] == children[index]['index'] == index
        assert case['case'] == children[index]['case'] == name
        assert case['profile'] == children[index]['profile'] == profile
        assert case['requestedExit'] == (23 if name == 'reported-error' else 0)
        assert case['delay'] == dict(fast=100, **{'slow-clean': 4000, 'reported-error': 100, 'hung': 15000})[name]
        assert case['exitCode'] == case['expectedExit'] == expected_exit
        assert case['handleClosed'] and case['passed'] and bounds[0] <= case['milliseconds'] <= bounds[1]
    report = dict(status='PASS', cases=cases, source=source, execution=execution,
                  scope='Process-only shutdown method. Retained C++ observer proves each child exit even if the outer sampler misses a short-lived process. No bridge IPC, renderer, callback or remote handle qualification.')
    with output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
    print(json.dumps(dict(status='PASS', cases=8, milliseconds=execution['milliseconds'], remaining=execution['remaining'])))


if __name__ == '__main__':
    main()
