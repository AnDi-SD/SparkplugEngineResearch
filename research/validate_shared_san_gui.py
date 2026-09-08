"""Check actual Viewer handler mesh positions against recorded PRS-backed FK."""
from pathlib import Path
import argparse
import hashlib
import json
import sys
import time
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
from san_pose_reference import expected


def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main():
    if '--' in sys.argv: sys.argv = [sys.argv[0], *sys.argv[sys.argv.index('--')+1:]]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('capture', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    started = time.perf_counter()
    source = json.loads(args.input.read_text(encoding='utf-8-sig'))
    capture = json.loads(args.capture.read_text(encoding='utf-8-sig'))
    assert capture['status'] == 'captured' and capture['inputSha256'] == digest(args.input)
    by_name = {row['name']: row for row in source['cases']}
    rows = []
    for case in capture['cases']:
        original = by_name[case['name']]
        reference = args.input.parent/original['reference']
        assert digest(reference) == original['referenceSha256']
        inputs = json.loads(reference.read_text(encoding='utf-8-sig'))
        for sample in case['samples']:
            ref_sample = original['samples'][sample['referenceSampleIndex']]
            assert abs(sample['seconds']-ref_sample['seconds']) < 1e-7
            target = expected(inputs, ref_sample['tracks'])
            extent = float(np.ptp(np.concatenate(list(target.values())), axis=0).max(initial=0))
            tolerance = 1e-5+extent*8e-6
            assert set(sample['meshes']) == set(target), 'Actual Viewer placement set differs from reference'
            for name, wanted in target.items():
                actual = np.array(sample['meshes'][name], dtype=np.float64)
                assert actual.shape == wanted.shape, 'Actual Viewer vertex count/order differs'
                error = float(np.linalg.norm(actual-wanted, axis=1).max(initial=0))
                rows.append({'case': case['name'], 'nativePrsReference': original['nativePrsReference'],
                    'seconds': sample['seconds'], 'mesh': name, 'vertices': len(wanted),
                    'maximumError': error, 'tolerance': tolerance, 'passed': error <= tolerance})
    passed = bool(rows) and all(row['passed'] for row in rows)
    report = {'status': 'passed' if passed else 'failed', 'inputSha256': digest(args.input),
        'captureSha256': digest(args.capture), 'cases': len(capture['cases']),
        'poses': sum(len(case['samples']) for case in capture['cases']), 'rows': rows,
        'maximumError': max(row['maximumError'] for row in rows), 'elapsedSeconds': time.perf_counter()-started,
        'scope': 'Actual WPF handler CPU mesh mirror; independent NumPy FK; GPU pixels and OS dialogs excluded.'}
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    assert passed, 'Viewer vertices differ from PC-PRS-backed FK; inspect per-mesh errors'
    print(f"PASS Viewer: {report['poses']} poses, {len(rows)} mesh/time checks, max error {report['maximumError']:.9g}")


if __name__ == '__main__': main()
