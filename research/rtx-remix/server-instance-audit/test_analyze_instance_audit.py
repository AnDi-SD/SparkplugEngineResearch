"""Bounded CPU-only checks of actual audit fixture logs and corrupt prefixes."""
import argparse
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
from unittest.mock import patch
import analyze_instance_audit as audit


def main(fixture, output):
    output.mkdir()  # A fresh directory is required; failures remain reviewable.
    (output / 'source').mkdir()
    for source in (Path(__file__), Path(audit.__file__)):
        (output / 'source' / source.name).write_bytes(source.read_bytes())
    checks = []

    def check(name, condition):
        if not condition:
            raise AssertionError(name)
        checks.append(name)

    def rejects(name, operation):
        try:
            operation()
        except ValueError:
            checks.append(name)
            return
        raise AssertionError('Did not reject: ' + name)

    actual = audit.analyze(fixture / 'process.json', fixture / 'sequence.jsonl')
    (output / 'actual-cpu-sequence.json').write_text(json.dumps(actual, indent=2) + '\n', encoding='utf-8')
    check('actual 16 returned calls conserved', actual['totals']['apiCalls'] == 16)
    check('actual 13 successes and 3 errors', actual['totals']['rendererApiSuccess'] == 13 and
          actual['totals']['rendererApiErrors'] == 3)
    check('actual 14 intervals and 7 Present boundaries', actual['intervals'] == 14 and actual['presentBoundaries'] == 7)
    check('actual incomplete preregistration retained', actual['boundaries']['registration_boundary'] == 1)
    check('actual qualified intervals separate', actual['categories']['qualifiedComplete']['intervals'] == 2)
    check('actual unexpected queue exit explicit', actual['termination'] == 'queue_exit_unexpected')
    check('actual both recorded PIDs exited', actual['recordedPidsExited'] and actual['immutableRead'])
    expected = {'cap.jsonl': 'capped', 'io-failure.jsonl': 'missing_terminal_record',
                'normal-exit.jsonl': 'queue_exit_normal'}
    for name, termination in expected.items():
        parsed = audit.validate((fixture / name).read_bytes())
        check('actual ' + name, parsed['termination'] == termination)
    raw = (fixture / 'sequence.jsonl').read_bytes()
    rows = [json.loads(line) for line in raw.splitlines()]

    def corrupt(name, mutate):
        changed = deepcopy(rows)
        mutate(changed)
        data = b''.join((json.dumps(row) + '\n').encode() for row in changed)
        rejects(name, lambda: audit.validate(data))

    cases = [
        ('cumulative mismatch', 2, 'totalCalls', 4),
        ('boolean counter', 2, 'apiCalls', True),
        ('negative counter', 2, 'earlyCalls', -1),
        ('success error partition', 2, 'rendererApiSuccess', 99),
        ('early outside calls', 2, 'earlyCalls', 4),
        ('invalid handles outside calls', 2, 'invalidMeshHandles', 99),
        ('first error absent', 2, 'firstRendererError', 0),
        ('noncontiguous interval', 2, 'interval', 3),
        ('decreasing epoch', 3, 'deviceEpoch', 0),
        ('incorrect Present increment', 3, 'presentIndex', 2),
        ('decreasing command sequence', 3, 'endSequence', 5),
        ('API outside interval', 3, 'firstApiSequence', 1),
        ('API last outside interval', 2, 'lastApiSequence', 99),
        ('draw outside interval', 2, 'firstDrawSequence', 99),
        ('unknown ordinary draw', 2, 'firstDrawKind', 9),
        ('zero draw subset', 2, 'zeroDrawCommands', 99),
        ('qualified foreign draw', 2, 'foreignDrawCommands', 1),
        ('qualified failed Present', 2, 'presentResult', -1),
        ('wrong registered device', 2, 'device', 999),
        ('wrong complete boundary', 2, 'complete', False),
        ('unknown boundary', 2, 'boundary', 'invented'),
        ('draw metadata without draw', 4, 'firstDrawSequence', 12),
        ('all early but late first draw', 2, 'firstDrawSequence', 1),
        ('unregistered qualification', 1, 'result', 7),
    ]
    for name, index, field, value in cases:
        corrupt(name, lambda changed, i=index, k=field, v=value: changed[i].__setitem__(k, v))
    corrupt('duplicate init', lambda changed: changed.insert(1, deepcopy(changed[0])))
    corrupt('cap not final', lambda changed: changed.insert(1, {'event': 'cap', 'truncated': True,
                                                             'furtherAuditDisabled': True}))
    corrupt('records after queue exit', lambda changed: changed.append(deepcopy(changed[1])))
    rejects('partial last line', lambda: audit.validate(raw[:-1]))
    rejects('oversized input', lambda: audit.validate(b' ' * (audit.MAX_BYTES + 1)))
    rejects('running PID', lambda: audit.exited(os.getpid()))
    launch = json.loads((fixture / 'process.json').read_text(encoding='utf-8-sig'))
    launch['pid'] = os.getpid()
    launch_path = output / 'live-launch-rejected.json'
    launch_path.write_text(json.dumps(launch), encoding='utf-8')
    rejects('live launch gate', lambda: audit.analyze(launch_path, fixture / 'sequence.jsonl'))
    changed = deepcopy(rows)
    changed[0]['pid'] = os.getpid()
    log_path = output / 'live-server-rejected.jsonl'
    log_path.write_bytes(b''.join((json.dumps(row) + '\n').encode() for row in changed))
    rejects('live server gate', lambda: audit.analyze(fixture / 'process.json', log_path))
    changing_path = output / 'mutated-log-rejected.jsonl'
    changing_path.write_bytes(raw)
    original_validate = audit.validate

    def change_after_parse(data):
        result = original_validate(data)
        with changing_path.open('ab') as stream:
            stream.write(b'\n')
        return result

    with patch.object(audit, 'validate', change_after_parse):
        rejects('immutable final fence', lambda: audit.analyze(fixture / 'process.json', changing_path))
    result = dict(status='PASS', checks=len(checks), cases=checks, gpu=False, nativeCode=False,
                  fixture=str(fixture), analyzerSha256=hashlib.sha256(Path(audit.__file__).read_bytes()).hexdigest().upper())
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    main(args.fixture, args.output)
