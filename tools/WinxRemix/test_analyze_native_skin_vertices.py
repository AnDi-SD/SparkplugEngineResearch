"""Bounded synthetic CPU tests for skin-vertex log accounting; no game/GPU input."""
import argparse
from contextlib import redirect_stderr, redirect_stdout
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import unittest
from unittest.mock import call, patch

import analyze_native_skin_vertices as analyzer
import analyze_native_skin_source as shared


SCOPE = ('sampled qualified Skin calls; exact CPU-source/upload and shared-layout '
         'comparison; no weighted API submission; unit-sum tolerance1e-5')
WRITER = (Path(__file__).resolve().parents[2] / 'local-data/rtx-remix/native-skin-vertex-tests/'
          'vertices-v1/source/research/rtx-remix/winx_native_skin_vertex_source.h')
CPU_WRITER_SAMPLES = WRITER.parents[3] / 'native-skin-vertices-fixture.jsonl'
ZERO = dict(vertices=0, influences=0, maxIndex=0, negativeWeightVertices=0,
            nonUnitSumVertices=0, maxSumError=0.0, minWeight=0.0, maxWeight=0.0)


def init(**changes):
    result = dict(event='init', schema=1, pid=123, enabled=True, maxLogBytes=16777216, scope=SCOPE)
    result.update(changes)
    return result


def sample(**changes):
    # These diagnostics are admissible observations, not successful skin submission.
    result = dict(event='sample', frame=300, skin=0x110000, mesh=0x120000, scene=0x130000,
                  modelCall=17, submission=100, accepted=True, rejection=0, generation=19,
                  vertices=3, influences=2, boneCount=4, maxIndex=3,
                  negativeWeightVertices=1, nonUnitSumVertices=1, maxSumError=0.125,
                  minWeight=-0.25, maxWeight=1.25)
    result.update(changes)
    return result


def frame(**changes):
    result = dict(event='frame', frame=300, attempts=3, matched=1, rejected=2)
    result.update(changes)
    return result


def rows():
    return [init(), sample(), sample(submission=101, accepted=False, rejection=3, generation=0, **ZERO),
            sample(submission=102, accepted=False, rejection=6, generation=0), frame(),
            frame(frame=601, attempts=0, matched=0, rejected=0)]


def encode(records):
    return b''.join((json.dumps(row, separators=(',', ':'), allow_nan=False) + '\n').encode('utf-8')
                    for row in records)


class SkinVertexTests(unittest.TestCase):
    cases = []
    directory = None

    def check(self, label, condition):
        self.cases.append(label)
        self.assertTrue(condition, label)

    def reject(self, label, action):
        self.cases.append(label)
        with self.assertRaises(ValueError, msg=label):
            action()

    def mutate(self, index, **changes):
        records = rows()
        records[index].update(changes)
        return records

    def test_positive_accounting_and_diagnostics(self):
        records = rows(); before = deepcopy(records)
        result = analyzer.summarize(records)
        self.check('diagnostics do not turn accepted samples into FAIL', result['status'] == 'PASS'
                   and result['totals'] == dict(attempts=3, matched=1, rejected=2))
        self.check('sparse frames and read-only records', result['frameRange'] == [300, 601] and records == before)
        decoded = analyzer.validate(encode(records))
        self.check('actual byte validator agrees with pure summary',
                   all(decoded[key] == value for key, value in result.items()))
        for reason in [1, 2, 4, 5, 7]:
            changed = self.mutate(2, rejection=reason)
            self.check('zero summary rejection ' + str(reason), analyzer.summarize(changed)['status'] == 'PASS')
        self.check('generic exception may retain completed Inspect summary',
                   analyzer.summarize(self.mutate(3, rejection=7))['status'] == 'PASS')
        changed = rows()
        changed[1].update(influences=1, boneCount=1, maxIndex=0, vertices=65536,
                          minWeight=1.0, maxWeight=1.0, maxSumError=0.0,
                          negativeWeightVertices=0, nonUnitSumVertices=0)
        changed[3].update(influences=4, boneCount=256, maxIndex=255)
        self.check('inclusive vertex influence and palette endpoints', analyzer.summarize(changed)['status'] == 'PASS')
        changed = rows()
        for offset in [1, 2, 3]:
            changed[offset].update(modelCall=2**32 + 17, submission=2**32 + 100 + offset)
        changed[1]['generation'] = 2**32 + 19
        self.check('uint64 sequences generation and repeated modelCall', analyzer.summarize(changed)['status'] == 'PASS')

    def test_empty_and_incomplete_logs_have_no_extra_completed_credit(self):
        result = analyzer.summarize([init()])
        self.check('init-only has no completed credit', result['frameRange'] == [None, None]
                   and not any(result['totals'].values()))
        base = analyzer.summarize(rows())
        pending = sample(frame=602, modelCall=18, submission=200)
        result = analyzer.summarize(rows() + [pending])
        self.check('terminal suffix explicit and excluded from counters', result['totals'] == base['totals']
                   and result['frameRange'] == [300, 601] and result['incompleteSampleSuffix']['frame'] == 602
                   and result['sampledTotals']['attempts'] == 4
                   and result['acceptedSampleDiagnostics']['vertices'] == 6)
        result = analyzer.summarize([init(), sample()])
        self.check('first sample without frame remains incomplete', not any(result['totals'].values())
                   and result['incompleteSampleSuffix']['frame'] == 300)

    def test_corrupted_schema_counters_and_samples(self):
        cases = [
            ('schema bool', 0, dict(schema=True)),
            ('disabled observer', 0, dict(enabled=False)),
            ('unknown bound', 0, dict(maxLogBytes=4096)),
            ('wrong provenance scope', 0, dict(scope='other')),
            ('invalid init PID', 0, dict(pid=True)),
            ('lost attempt', 4, dict(attempts=2)),
            ('extra conserved samples', 4, dict(attempts=4, matched=2)),
            ('accepted rejected partition mismatch', 4, dict(matched=2, rejected=1)),
            ('boolean frame counter', 4, dict(attempts=True)),
            ('negative counter', 4, dict(rejected=-1)),
            ('uint32 counter overflow', 4, dict(rejected=2**32)),
            ('accepted flag must be bool', 1, dict(accepted=1)),
            ('accepted sample requires reason zero', 1, dict(rejection=1)),
            ('accepted sample requires generation', 1, dict(generation=0)),
            ('rejected sample cannot retain generation', 2, dict(generation=19)),
            ('rejected sample requires reason', 2, dict(rejection=0)),
            ('unknown rejection', 2, dict(rejection=8)),
            ('early failure cannot retain full summary', 1, dict(accepted=False, rejection=5, generation=0)),
            ('partial failure summary', 2, dict(vertices=1)),
            ('final-fence failure requires full summary', 3, dict(**ZERO)),
            ('vertices cap', 1, dict(vertices=65537)),
            ('zero accepted influences', 1, dict(influences=0)),
            ('influence cap', 1, dict(influences=5)),
            ('palette cap', 1, dict(boneCount=257)),
            ('bone index must be below palette count', 1, dict(maxIndex=4)),
            ('negative diagnostic count bound', 1, dict(negativeWeightVertices=4)),
            ('nonunit diagnostic count bound', 1, dict(nonUnitSumVertices=4)),
            ('negative diagnostic agrees with extrema', 1, dict(negativeWeightVertices=0)),
            ('nonunit diagnostic agrees with error', 1, dict(nonUnitSumVertices=0)),
            ('ordered weight range', 1, dict(minWeight=2.0)),
            ('finite weights', 1, dict(minWeight=float('nan'))),
            ('finite sum error', 1, dict(maxSumError=float('inf'))),
            ('nonnegative sum error', 1, dict(maxSumError=-0.1)),
            ('unsigned integer vertices', 1, dict(vertices=1.5)),
            ('positive sampled scene identity', 1, dict(scene=0)),
            ('generation width', 1, dict(generation=2**64)),
            ('strict submission order', 3, dict(submission=101)),
            ('nondecreasing modelCall', 3, dict(modelCall=16)),
        ]
        for label, index, changes in cases:
            with self.subTest(case=label):
                self.reject(label, lambda index=index, changes=changes:
                            analyzer.summarize(self.mutate(index, **changes)))

    def test_event_order_corruptions(self):
        cases = [('duplicate init', [init()] + rows()),
                 ('missing init', rows()[1:]),
                 ('duplicate completed frame', rows() + [rows()[-1]]),
                 ('late sample', rows() + [sample(frame=601, submission=200)]),
                 ('unclosed earlier sample frame', [init(), sample(), sample(frame=301, submission=101)]),
                 ('missing sample is not zero work', [rows()[0]] + rows()[2:]),
                 ('non-monotonic suffix submission', rows() + [sample(frame=602, modelCall=18, submission=102)])]
        for label, records in cases:
            with self.subTest(case=label):
                self.reject(label, lambda records=records: analyzer.summarize(records))

    def test_jsonl_boundaries(self):
        raw = encode(rows())
        cases = [('partial JSON row', raw[:-2]), ('missing final newline', raw[:-1]),
                 ('duplicate JSON key', raw.replace(b'"schema":1', b'"schema":1,"schema":1', 1)),
                 ('nonfinite JSON token', raw.replace(b'"maxSumError":0.125', b'"maxSumError":NaN', 1)),
                 ('blank record', raw + b'\n'), ('invalid UTF8', b'\xff\n' + raw),
                 ('valid JSON row over bound', encode([init(scope=SCOPE + ' ' * 4096)])),
                 ('oversized log', b'x' * (16 * 1024 * 1024 + 4097))]
        for label, data in cases:
            with self.subTest(case=label):
                self.reject(label, lambda data=data: analyzer.validate(data))

    def test_cap_suffix_and_record_start_bound(self):
        pieces = [encode([init()])]
        target = 16 * 1024 * 1024 - 1
        full = (target - len(pieces[0])) // 4096 - 1

        def padding(number, length):
            row = encode([frame(frame=number, attempts=0, matched=0, rejected=0)])
            assert len(row) <= length <= 4096
            return row[:-1] + b' ' * (length - len(row)) + b'\n'

        for number in range(full):
            pieces.append(padding(number, 4096))
        remaining = target - len(pieces[0]) - full * 4096
        pieces.append(padding(full, remaining // 2))
        pieces.append(padding(full + 1, remaining - remaining // 2))
        pieces.append(encode([sample(frame=full + 2)]))
        data = b''.join(pieces)
        result = analyzer.validate(data)
        self.check('one final sample may cross cap without cap marker', result['logLimitReached']
                   and result['logCapMarker'] is False and not any(result['totals'].values())
                   and result['incompleteSampleSuffix']['frame'] == full + 2)
        self.reject('no record begins after producer cap',
                    lambda: analyzer.validate(data + encode([frame(frame=full + 2)])))

    def test_actual_frozen_writer_samples_with_independent_framing(self):
        raw_lines = CPU_WRITER_SAMPLES.read_bytes().splitlines(keepends=True)
        self.check('frozen CPU writer fixture has seventeen samples', len(raw_lines) == 17)
        for number, raw_line in enumerate(raw_lines):
            original = shared.decode(raw_line)
            # Keep each production payload byte-for-byte. Only framing is synthetic;
            # fixture cases deliberately reset sequences, so this is no continuous run.
            matched = int(original['accepted'])
            wrapper = encode([init()]) + raw_line + encode([frame(frame=original['frame'],
                       attempts=1, matched=matched, rejected=1 - matched)])
            result = analyzer.validate(wrapper)
            with self.subTest(sample=number):
                self.check('actual frozen writer sample ' + str(number), result['status'] == 'PASS'
                           and result['totals'] == dict(attempts=1, matched=matched, rejected=1 - matched))
        self.reject('independent CPU cases cannot masquerade as a continuous run',
                    lambda: analyzer.validate(encode([init()]) + b''.join(raw_lines)))

    def test_failure_history_does_not_truncate_sample_or_frame_counts(self):
        records = [init()] + [sample(submission=100 + n, accepted=False, rejection=3, generation=0, **ZERO)
                               for n in range(70)] + [frame(attempts=70, matched=0, rejected=70)]
        result = analyzer.summarize(records)
        self.check('bounded failure history retains all accounting', result['failureSamples'] == 70
                   and len(result['failureHistory']) == 64 and result['failureHistoryTruncated']
                   and result['totals'] == dict(attempts=70, matched=0, rejected=70)
                   and result['failureHistory'] == records[1:65])

    def own_run(self, name, log_pid=123):
        directory = self.directory / name
        directory.mkdir()
        launch = directory / 'launch.json'
        log = directory / 'native-skin-vertices.jsonl'
        launch.write_text(json.dumps(dict(pid=123, nativeSkinVertices=True, proxySha256='0' * 64)), encoding='utf-8')
        records = rows(); records[0]['pid'] = log_pid
        log.write_bytes(encode(records))
        return directory, launch, log

    def test_closed_file_and_process_gates(self):
        directory, _, log = self.own_run('valid')
        with patch.object(analyzer, 'exited') as exited:
            result = analyzer.analyze(directory)
            self.check('closed immutable bytes and both PID gates', result['immutableRead'] and result['pidExited']
                       and exited.call_args_list == [call(123), call(123)]
                       and result['sourceLogSha256'] == hashlib.sha256(log.read_bytes()).hexdigest().upper())
        directory, _, _ = self.own_run('pid-mismatch', log_pid=124)
        with patch.object(analyzer, 'exited'):
            self.reject('launch/log PID mismatch', lambda: analyzer.analyze(directory))
        directory, _, _ = self.own_run('live-gate')
        with (patch.object(analyzer, 'exited', side_effect=ValueError('mock process running')),
              patch.object(analyzer, 'read_closed', wraps=analyzer.read_closed) as read):
            self.reject('live PID blocks observer read', lambda: analyzer.analyze(directory))
            self.check('live PID gate opens only metadata', read.call_count == 1 and read.call_args[0][0].name == 'launch.json')
        for name in ['log', 'launch']:
            directory, launch, log = self.own_run('mutated-' + name)
            count = 0

            def mutate_after_parse(pid):
                nonlocal count
                count += 1
                if count == 2:
                    with (log if name == 'log' else launch).open('ab') as stream:
                        stream.write(b'\n')

            with patch.object(analyzer, 'exited', side_effect=mutate_after_parse):
                self.reject('final immutable ' + name + ' fence', lambda: analyzer.analyze(directory))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture-dir', type=Path, required=True)
    args = parser.parse_args()
    args.fixture_dir.mkdir(parents=True, exist_ok=False)
    sources = args.fixture_dir / 'source'
    sources.mkdir()
    hashes = {}
    for path in [Path(__file__), Path(analyzer.__file__), Path(shared.__file__), WRITER, CPU_WRITER_SAMPLES]:
        data = path.read_bytes()
        (sources / path.name).write_bytes(data)
        hashes[path.name] = hashlib.sha256(data).hexdigest().upper()
    SkinVertexTests.directory = args.fixture_dir
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SkinVertexTests)
    with ((args.fixture_dir / 'stdout.log').open('w', encoding='utf-8') as stdout,
          (args.fixture_dir / 'stderr.log').open('w', encoding='utf-8') as stderr,
          redirect_stdout(stdout), redirect_stderr(stderr)):
        result = unittest.TextTestRunner(verbosity=2, stream=stderr).run(suite)
        report = dict(status='PASS' if result.wasSuccessful() else 'FAIL', tests=result.testsRun,
                      checks=len(SkinVertexTests.cases), cases=SkinVertexTests.cases,
                      failures=len(result.failures), errors=len(result.errors),
                      gpu=False, nativeCode=False, gameExecuted=False, sourceSha256=hashes)
        print(json.dumps(report))
    (args.fixture_dir / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: report[key] for key in ['status', 'tests', 'checks', 'failures', 'errors', 'sourceSha256']}))
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
