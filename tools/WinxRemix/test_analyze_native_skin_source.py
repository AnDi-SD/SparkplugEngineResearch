"""Synthetic CPU tests for strict native-skin JSONL accounting and closed-file gates.

No game log is read. File-boundary cases create and retain only their own inputs.
"""
import argparse
from contextlib import redirect_stderr, redirect_stdout
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import unittest
from unittest.mock import call, patch

import analyze_native_skin_source as analyzer


REASONS = ['scope', 'callsite', 'scene', 'registry', 'support', 'skin', 'mesh',
           'palette', 'bone', 'nonfinite', 'changed']
SCOPE = ('own Skin mesh call; current plain Node cached world PRS; bounded tolerant '
         'palette observer, not weighted geometry submission or durable lifetime')


def initialization(**changes):
    row = dict(event='init', schema=1, enabled=True, imageVerified=True, pid=123,
               maxLogBytes=16777216, maxBones=256, absoluteTolerance=0.0001,
               relativeTolerance=0.00001, rejectionNames=list(REASONS), scope=SCOPE)
    row.update(changes)
    return row


def comparison(**changes):
    row = dict(event='compare', frame=300, scene=0x110000, skin=0x120000,
               support=0x130000, camera=0x140000, mesh=0x150000, renderer=0x160000,
               palette=0x170000, boneCount=2, weightHint=2, meshWeights=2,
               modelCall=17, submission=101, withinTolerance=True,
               bitDifferences=0, maxAbsoluteError=0.0, maxRelativeError=0.0)
    row.update(changes)
    return row


def completed(**changes):
    # One omitted matching sample, one emitted match, one emitted mismatch,
    # and one rejection from each reason. Calls are not a mesh-attempt count.
    row = dict(event='frame', frame=300, calls=5, callFailures=1, withoutMesh=1,
               attempts=14, matched=2, mismatches=1, bones=6, bitDifferences=2,
               maxAbsoluteError=0.1, maxRelativeError=0.2, rejected=[1] * 11)
    row.update(changes)
    return row


def rows():
    return [initialization(), comparison(),
            comparison(submission=102, withinTolerance=False, bitDifferences=2,
                       maxAbsoluteError=0.1, maxRelativeError=0.2), completed(),
            completed(frame=601, calls=1, callFailures=1, withoutMesh=1,
                      attempts=0, matched=0, mismatches=0, bones=0, bitDifferences=0,
                      maxAbsoluteError=0.0, maxRelativeError=0.0, rejected=[0] * 11)]


def encode(records):
    return b''.join((json.dumps(row, separators=(',', ':'), allow_nan=False) + '\n').encode('utf-8')
                    for row in records)


class SkinAnalyzerTests(unittest.TestCase):
    checks = 0
    fixture_directory = None

    def checked(self, condition, message):
        type(self).checks += 1
        self.assertTrue(condition, message)

    def reject(self, action, message):
        type(self).checks += 1
        with self.assertRaises(ValueError, msg=message):
            action()

    def corrupt(self, index, field, value):
        records = rows()
        records[index][field] = deepcopy(value)
        return records

    def test_completed_totals_and_sparse_samples(self):
        records = rows()
        unchanged = deepcopy(records)
        result = analyzer.summarize(records)
        self.checked(result['status'] == 'PASS', 'valid rows pass')
        self.checked(result['frameRange'] == [300, 601], 'frame gaps are preserved')
        self.checked(result['totals']['attempts'] == 14 and result['totals']['matched'] == 2
                     and result['totals']['mismatches'] == 1, 'completed counters conserve attempts')
        self.checked(result['totals']['calls'] == 6 and result['totals']['callFailures'] == 2
                     and result['totals']['withoutMesh'] == 2, 'original call counters stay separate')
        self.checked(result['incompleteSampleSuffix'] is None, 'no invented incomplete suffix')
        self.checked(records == unchanged, 'summarize leaves caller records unchanged')
        decoded = analyzer.validate(encode(records))
        self.checked(all(decoded[key] == value for key, value in result.items()),
                     'byte decoding agrees with pure summary')
        # Matching samples can be omitted; completed mismatch samples cannot.
        sparse = analyzer.summarize([records[0], records[2], records[3], records[4]])
        self.checked(sparse['totals'] == result['totals'], 'samples are a subset, not the frame denominator')
        self.reject(lambda: analyzer.summarize([records[0], records[3], records[4]]),
                    'completed frame cannot lose its mandatory mismatch sample')

    def test_initialization_only_has_no_coverage_credit(self):
        for enabled, verified in [(True, True), (False, True), (False, False)]:
            result = analyzer.summarize([initialization(enabled=enabled, imageVerified=verified)])
            self.checked(result['completedFrames'] == 0 and result['frameRange'] == [None, None]
                         and not any(result['totals'].values()) and result['incompleteSampleSuffix'] is None,
                         'init-only evidence has no completed frame or draw credit')
        result = analyzer.summarize([initialization(), comparison()])
        self.checked(result['completedFrames'] == 0 and not any(result['totals'].values())
                     and result['incompleteSampleSuffix']['frame'] == 300,
                     'pre-first-frame sample remains incomplete')

    def test_tolerant_bit_difference_is_not_mismatch(self):
        sample = comparison(bitDifferences=1)
        frame = completed(calls=1, callFailures=0, withoutMesh=0, attempts=1, matched=1,
                          mismatches=0, bones=2, bitDifferences=1, maxAbsoluteError=0.0,
                          maxRelativeError=0.0, rejected=[0] * 11)
        result = analyzer.summarize([initialization(), sample, frame])
        self.checked(result['totals']['matched'] == 1 and result['totals']['mismatches'] == 0,
                     'signed-zero bit differences preserve the explicit tolerance outcome')

    def test_pending_sample_suffix_does_not_inflate_completed_totals(self):
        base = analyzer.summarize(rows())
        result = analyzer.summarize(rows() + [comparison(frame=602, modelCall=18, submission=200),
            comparison(frame=602, modelCall=18, submission=201, withinTolerance=False,
                       bitDifferences=1, maxAbsoluteError=0.1, maxRelativeError=0.2)])
        self.checked(result['totals'] == base['totals'], 'incomplete frame is not completed evidence')
        self.checked(result['incompleteSampleSuffix']['frame'] == 602, 'pending suffix is explicit')
        self.checked(result['frameRange'] == [300, 601], 'pending frame does not extend completed range')

    def test_initialization_contract(self):
        invalid = [rows()[1:], [], [initialization(), initialization()] + rows()[1:],
                   rows() + [initialization()], [None], [list()]]
        fields = [('schema', True), ('schema', 2), ('enabled', False), ('enabled', 1),
                  ('imageVerified', False), ('imageVerified', 1), ('pid', False), ('pid', 0),
                  ('maxLogBytes', 4096), ('maxBones', 257), ('absoluteTolerance', 0.01),
                  ('relativeTolerance', 0.01), ('rejectionNames', REASONS[:-1]),
                  ('rejectionNames', list(reversed(REASONS))),
                  ('rejectionNames', REASONS[:-1] + ['palette']), ('scope', 'other'), ('scope', True)]
        invalid += [self.corrupt(0, key, value) for key, value in fields]
        for index, records in enumerate(invalid):
            with self.subTest(index=index):
                self.reject(lambda records=records: analyzer.summarize(records), 'invalid initialization')

    def test_counter_conservation_and_rejection_shape(self):
        fields = [('attempts', 13), ('attempts', 15), ('callFailures', 6), ('withoutMesh', 6),
                  ('rejected', [1] * 10), ('rejected', [1] * 12), ('rejected', {'scope': 11}),
                  ('rejected', [True] + [1] * 10), ('rejected', [-1] + [1] * 10),
                  ('rejected', [0x100000000] + [1] * 10), ('bones', 2),
                  ('bones', 3 * 256 + 1), ('bitDifferences', 6 * 16 + 1)]
        for field, value in fields:
            with self.subTest(field=field, value=value):
                self.reject(lambda field=field, value=value: analyzer.summarize(self.corrupt(3, field, value)),
                            'invalid completed counter relationship')

    def test_unsigned_fields_reject_bool_negative_fraction_and_overflow(self):
        frame_fields = ['frame', 'calls', 'callFailures', 'withoutMesh', 'attempts', 'matched',
                        'mismatches', 'bones', 'bitDifferences']
        sample_fields = ['frame', 'scene', 'skin', 'support', 'camera', 'mesh', 'renderer', 'palette',
                         'boneCount', 'weightHint', 'meshWeights', 'bitDifferences']
        for index, fields in [(3, frame_fields), (1, sample_fields)]:
            for field in fields:
                for value in [True, -1, 1.5, 0x100000000]:
                    with self.subTest(index=index, field=field, value=value):
                        self.reject(lambda index=index, field=field, value=value:
                                    analyzer.summarize(self.corrupt(index, field, value)), 'invalid uint32')
        for field in ['modelCall', 'submission']:
            for value in [True, -1, 1.5, 0, 0x10000000000000000]:
                with self.subTest(field=field, value=value):
                    self.reject(lambda field=field, value=value: analyzer.summarize(self.corrupt(1, field, value)),
                                'invalid positive uint64 sequence')

    def test_comparison_payload_and_frame_subsets(self):
        fields = [('boneCount', 0), ('boneCount', 257), ('bitDifferences', 33),
                  ('withinTolerance', 1), ('withinTolerance', None), ('scene', 0), ('skin', 0),
                  ('support', 0), ('camera', 0), ('mesh', 0), ('renderer', 0), ('palette', 0)]
        for field, value in fields:
            with self.subTest(field=field, value=value):
                self.reject(lambda field=field, value=value: analyzer.summarize(self.corrupt(1, field, value)),
                            'invalid comparison payload')
        # Keep aggregate conservation valid while making a sampled subset exceed it.
        invalid = [self.corrupt(3, 'bitDifferences', 1), self.corrupt(3, 'maxAbsoluteError', 0.01),
                   self.corrupt(3, 'maxRelativeError', 0.01)]
        too_few = rows(); too_few[3].update(matched=0, attempts=12, bones=2)
        invalid.append(too_few)
        too_few = rows(); too_few[3].update(mismatches=0, attempts=13, bones=4)
        invalid.append(too_few)
        for records in invalid:
            self.reject(lambda records=records: analyzer.summarize(records), 'samples exceed completed frame')

    def test_nonfinite_or_negative_errors(self):
        for index in [1, 3]:
            for field in ['maxAbsoluteError', 'maxRelativeError']:
                for value in [float('nan'), float('inf'), float('-inf'), -0.1, True, '0.0', 10**1000]:
                    with self.subTest(index=index, field=field, value=value):
                        self.reject(lambda index=index, field=field, value=value:
                                    analyzer.summarize(self.corrupt(index, field, value)), 'invalid finite error')

    def test_event_and_sequence_order(self):
        invalid = [rows() + [rows()[-1]], rows() + [comparison(frame=601, submission=200)],
                   [initialization(), comparison(), completed(frame=301)],
                   [initialization(), comparison(), comparison(frame=301, submission=102)],
                   self.corrupt(2, 'submission', 101), self.corrupt(2, 'submission', 100),
                   self.corrupt(2, 'modelCall', 16), self.corrupt(4, 'frame', 299),
                   self.corrupt(4, 'event', 'cap'),
                   rows() + [comparison(frame=602, modelCall=16, submission=200)],
                   rows() + [comparison(frame=602, modelCall=18, submission=102)]]
        for records in invalid:
            self.reject(lambda records=records: analyzer.summarize(records), 'invalid event/sequence order')
        # Sequence widths are genuinely uint64, not uint32.
        records = rows(); records[1].update(modelCall=2**32 + 1, submission=2**32 + 2)
        records[2].update(modelCall=2**32 + 1, submission=2**32 + 3)
        self.checked(analyzer.summarize(records)['status'] == 'PASS', 'wide sequences and repeated modelCall pass')

    def test_jsonl_corruption(self):
        raw = encode(rows())
        invalid = [raw[:-1], raw + b'{', raw + b'\n', b'', raw + b'null\n', raw + b'[]\n',
                   raw.replace(b'"schema":1', b'"schema":1,"schema":1', 1),
                   raw.replace(b'"maxAbsoluteError":0.0', b'"maxAbsoluteError":NaN', 1),
                   raw.replace(b'"maxAbsoluteError":0.0', b'"maxAbsoluteError":Infinity', 1),
                   raw.replace(b'"maxAbsoluteError":0.0', b'"maxAbsoluteError":1e999', 1),
                   b'\xff\n' + raw, raw + b'x' * (4096 + 1) + b'\n',
                   b'x' * (16 * 1024 * 1024 + 4096 + 1)]
        for index, data in enumerate(invalid):
            with self.subTest(index=index):
                self.reject(lambda data=data: analyzer.validate(data), 'invalid JSONL boundary')

    def test_one_row_cap_overshoot_and_missing_cap_marker(self):
        # Whitespace padding controls byte boundaries without inventing schema fields.
        raw_init = encode([initialization()])
        target = 16 * 1024 * 1024 - 1
        full_rows = (target - len(raw_init)) // 4096 - 1
        pieces = [raw_init]

        def blank_frame(number, length):
            raw = encode([completed(frame=number, calls=0, callFailures=0, withoutMesh=0,
                                   attempts=0, matched=0, mismatches=0, bones=0, bitDifferences=0,
                                   maxAbsoluteError=0.0, maxRelativeError=0.0, rejected=[0] * 11)])
            assert len(raw) <= length <= 4096, 'synthetic padded frame fits the actual row bound'
            return raw[:-1] + b' ' * (length - len(raw)) + b'\n'

        for number in range(full_rows):
            pieces.append(blank_frame(number, 4096))
        remaining = target - len(raw_init) - full_rows * 4096
        pieces.append(blank_frame(full_rows, remaining // 2))
        pieces.append(blank_frame(full_rows + 1, remaining - remaining // 2))
        pieces.append(encode([comparison(frame=full_rows + 2)]))
        data = b''.join(pieces)
        result = analyzer.validate(data)
        self.checked(result['logLimitReached'] and result['logCapMarker'] is False,
                     'cap reach is byte evidence without an invented producer cap marker')
        self.checked(result['incompleteSampleSuffix']['frame'] == full_rows + 2
                     and result['completedFrames'] == full_rows + 2 and not any(result['totals'].values()),
                     'last allowed compare can cross cap without becoming a completed frame')
        self.reject(lambda: analyzer.validate(data + encode([completed(frame=full_rows + 3)])),
                    'no additional record may begin after the producer cap')
        oversized_init = raw_init[:-1] + b' ' * (4097 - len(raw_init)) + b'\n'
        self.reject(lambda: analyzer.validate(oversized_init), 'valid JSON row still has a byte bound')

    def test_failure_history_is_bounded_without_losing_totals(self):
        records = [initialization()] + [completed(frame=number, calls=1, callFailures=1,
                    withoutMesh=1, attempts=0, matched=0, mismatches=0, bones=0, bitDifferences=0,
                    maxAbsoluteError=0.0, maxRelativeError=0.0, rejected=[0] * 11)
                    for number in range(70)]
        result = analyzer.summarize(records)
        self.checked(result['failureFrameCount'] == 70 and len(result['failureFrameHistory']) == 64
                     and result['failureFrameHistoryTruncated'], 'failure samples are bounded independently of counters')
        self.checked(result['totals']['calls'] == 70 and result['totals']['callFailures'] == 70
                     and result['totals']['withoutMesh'] == 70, 'bounded history does not truncate totals')

    def own_run(self, name, pid=123, log_pid=123, enabled=True):
        directory = self.fixture_directory / name
        directory.mkdir()
        launch = directory / 'launch.json'
        log = directory / 'native-skin-source.jsonl'
        launch.write_text(json.dumps(dict(pid=pid, nativeSkinSource=enabled, proxySha256='0' * 64)), encoding='utf-8')
        records = rows(); records[0]['pid'] = log_pid
        log.write_bytes(encode(records))
        return directory, launch, log

    def test_closed_pid_and_immutable_file_gates(self):
        directory, launch, log = self.own_run('closed-valid')
        raw = log.read_bytes()
        with patch.object(analyzer, 'exited') as exited:
            result = analyzer.analyze(directory)
            self.checked(exited.call_args_list == [call(123), call(123)],
                         'recorded process closure checked before and after parsing')
            self.checked(result['immutableRead'] and result['pidExited']
                         and result['sourceLogSha256'] == hashlib.sha256(raw).hexdigest().upper(),
                         'closed evidence reports exact bytes and immutable read')
        directory, _, _ = self.own_run('launch-log-pid-mismatch', log_pid=124)
        with patch.object(analyzer, 'exited'):
            self.reject(lambda: analyzer.analyze(directory), 'launch/log PID mismatch')
        directory, _, _ = self.own_run('launch-disabled', enabled=False)
        with patch.object(analyzer, 'exited'):
            self.reject(lambda: analyzer.analyze(directory), 'launch opt-in disabled')
        directory, _, _ = self.own_run('live-pid-gate')
        original_read = analyzer.read_closed
        with (patch.object(analyzer, 'exited', side_effect=ValueError('mock recorded process is alive')),
              patch.object(analyzer, 'read_closed', wraps=original_read) as read):
            self.reject(lambda: analyzer.analyze(directory), 'live recorded PID blocks analysis')
            self.checked(read.call_count == 1 and read.call_args[0][0].name == 'launch.json',
                         'live PID gate prevents opening the observer log')
        directory, _, _ = self.own_run('final-pid-gate')
        with patch.object(analyzer, 'exited', side_effect=[None, ValueError('mock PID reused')]):
            self.reject(lambda: analyzer.analyze(directory), 'final PID gate rejects process reuse')
        for name in ['log', 'launch']:
            directory, launch, log = self.own_run('immutable-' + name)
            calls = 0

            def changed_on_final_check(pid):
                nonlocal calls
                calls += 1
                if calls == 2:
                    with (log if name == 'log' else launch).open('ab') as stream:
                        stream.write(b'\n')

            with patch.object(analyzer, 'exited', side_effect=changed_on_final_check):
                self.reject(lambda: analyzer.analyze(directory), 'final immutable fence checks ' + name)
        oversized = self.fixture_directory / 'owned-oversized.bin'
        oversized.write_bytes(b'123456789')
        self.reject(lambda: analyzer.read_closed(oversized, 8), 'read boundary rejects oversized owned file')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture-dir', type=Path, required=True)
    args = parser.parse_args()
    args.fixture_dir.mkdir(parents=True, exist_ok=False)
    source = args.fixture_dir / 'source'
    source.mkdir()
    for path in [Path(__file__), Path(analyzer.__file__)]:
        (source / path.name).write_bytes(path.read_bytes())
    SkinAnalyzerTests.fixture_directory = args.fixture_dir
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SkinAnalyzerTests)
    with ((args.fixture_dir / 'stdout.log').open('w', encoding='utf-8') as stdout,
          (args.fixture_dir / 'stderr.log').open('w', encoding='utf-8') as stderr,
          redirect_stdout(stdout), redirect_stderr(stderr)):
        result = unittest.TextTestRunner(verbosity=2, stream=stderr).run(suite)
    report = dict(status='PASS' if result.wasSuccessful() else 'FAIL', tests=result.testsRun,
                  checks=SkinAnalyzerTests.checks, failures=len(result.failures), errors=len(result.errors),
                  gpu=False, nativeCode=False, gameExecuted=False,
                  analyzerSha256=hashlib.sha256(Path(analyzer.__file__).read_bytes()).hexdigest().upper())
    (args.fixture_dir / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
