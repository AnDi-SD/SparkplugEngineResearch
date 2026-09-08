"""Compare preparation results and verified SMO bytes from the same benchmark."""
from pathlib import Path
import argparse
import json
import statistics
from validate_importer_performance import digest


def compare(before_path, after_path):
    reports = [json.loads(path.read_text(encoding='utf-8')) for path in (before_path, after_path)]
    for report in reports:
        assert report['kind'] == 'importer-preparation-benchmark' and report['status'] == 'passed'
        assert report['warmups'] == 1 and 1 <= report['iterations'] <= 7
        assert len(report['rows']) == report['iterations'] + 1
        assert digest(report['targetPath']) == report['targetSha256']
        assert digest(report['donorPath']) == report['donorSha256']
        assert digest(report['output']) == report['outputSha256']
        assert len({r['fingerprint'] for r in report['rows']}) == 1
    before, after = reports
    for key in ('mode', 'targetSha256', 'donorSha256', 'iterations', 'warmups',
                'alignmentMode', 'alignment', 'poseSource', 'analysis', 'scope',
                'benchmarkAssemblySha256', 'outputSha256'):
        assert before[key] == after[key], key
    assert before['rows'][0]['fingerprint'] == after['rows'][0]['fingerprint']
    metrics = {}
    for key in ('prepareMs', 'prepareCpuMs', 'prepareBytes'):
        old, new = [statistics.median(row[key] for row in report['rows'] if not row['warmup'])
                    for report in reports]
        metrics[key] = dict(before=old, after=new, reductionPercent=100*(1-new/old))
    return dict(kind='importer-preparation-comparison', status='passed', metrics=metrics,
                before=dict(path=before_path.as_posix(), sha256=digest(before_path)),
                after=dict(path=after_path.as_posix(), sha256=digest(after_path)),
                peakBefore=before['peakWorkingSet'], peakAfter=after['peakWorkingSet'],
                benchmarkAssemblySha256=before['benchmarkAssemblySha256'],
                beforeCoreSha256=before['coreAssemblySha256'], afterCoreSha256=after['coreAssemblySha256'],
                publicPreparationSha256=after['rows'][0]['fingerprint'], outputSha256=after['outputSha256'],
                limits=['Shared host and quantized Windows CPU counters.',
                        'Cumulative allocation reductions do not imply lower peak resident memory.',
                        'Preparation only: separate GUI plan analysis, drawing and writing are outside the timed section.',
                        'Equal public preparation and SMO bytes establish unchanged behavior for these cases, not artistic weight quality.'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    assert not args.output.exists(), 'Use a new report path'
    result = compare(args.before, args.after)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=True))
