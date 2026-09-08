"""Compare bounded, identical-harness Importer runs and verify every output hash."""
from pathlib import Path
import argparse
import hashlib
import json
import statistics


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest().upper()


def compare(before_path, after_path):
    reports = []
    for path in (before_path, after_path):
        data = json.loads(path.read_text(encoding='utf-8'))
        assert data['status'] == 'passed' and data['kind'] == 'importer-real-donor-benchmark'
        assert data['warmups'] == 2 and 1 <= data['iterations'] <= 9
        assert len(data['rows']) == data['iterations'] + data['warmups']
        assert digest(data['targetPath']) == data['targetSha256']
        assert digest(data['donorPath']) == data['donorSha256']
        assert len({row['sha256'] for row in data['rows']}) == 1
        for row in data['rows']:
            assert digest(row['output']) == row['sha256']
            assert Path(row['output']).stat().st_size == row['FileSize']
        rows = [row for row in data['rows'] if not row['warmup']]
        assert len(rows) == data['iterations']
        metrics = {key: statistics.median(row[key] for row in rows)
                   for key in ('writeMs', 'writeCpuMs', 'writeBytes', 'analyzeMs', 'previewMs')}
        metrics['peakWorkingSet'] = data['peakWorkingSet']
        reports.append((data, metrics))
    before, after = (row[0] for row in reports)
    for key in ('targetSha256', 'donorSha256', 'meshes', 'triangles', 'textures',
                'iterations', 'warmups', 'benchmarkAssemblySha256', 'scope'):
        assert before[key] == after[key], key
    assert before['rows'][0]['sha256'] == after['rows'][0]['sha256']
    old_metrics, new_metrics = (row[1] for row in reports)
    return dict(kind='importer-performance-comparison', status='passed',
                before=dict(path=before_path.as_posix(), sha256=digest(before_path)),
                after=dict(path=after_path.as_posix(), sha256=digest(after_path)),
                benchmarkAssemblySha256=before['benchmarkAssemblySha256'],
                beforeCoreSha256=before['coreAssemblySha256'], afterCoreSha256=after['coreAssemblySha256'],
                outputSha256=after['rows'][0]['sha256'], verifiedOutputs=len(before['rows']) + len(after['rows']),
                beforeMetrics=old_metrics, afterMetrics=new_metrics,
                reductionPercent={key: 100 * (1 - new_metrics[key] / old_metrics[key])
                                  for key in old_metrics},
                scope=after['scope'],
                limits=['Same benchmark executable; separate process runs on a shared Windows host.',
                        'CPU counter is quantized; timing is a sample, not an end-user latency guarantee.',
                        'Allocated bytes are cumulative during verified writing, not resident memory.',
                        'Output equality carries forward existing validation only for the exact same bytes.'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    assert not args.output.exists(), 'Use a new report path'
    result = compare(args.before, args.after)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=True, indent=2))
