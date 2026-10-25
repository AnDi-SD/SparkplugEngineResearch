"""Qualify the offline observation reader with known world rays and incomplete data."""
import argparse
import json
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analyzer', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    analyzer = args.analyzer.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=False)
    world = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 3, 1]
    data = (struct.pack('<2I4IQ16f9f', 0x57475031, 42, 1, 7, 1, 3, 123, *world,
                        -1, -1, 2, 1, -1, 2, 0, 1, 2) + struct.pack('<4I', 0, 1, 1, 0))
    checks = []

    def run(name, geometry, origins):
        binary, points = args.output / (name + '.bin'), args.output / (name + '.txt')
        binary.write_bytes(geometry)
        points.write_text(origins, encoding='ascii')
        result = subprocess.run([str(analyzer), str(binary), str(points)], capture_output=True, text=True, timeout=15)
        (args.output / (name + '.stdout')).write_text(result.stdout, encoding='utf-8')
        (args.output / (name + '.stderr')).write_text(result.stderr, encoding='utf-8')
        return result

    result = run('world-rays', data, '0 0 0 0\n1 0 0 6\n2 3 0 0\n')
    if result.returncode:
        raise RuntimeError(result.stderr)
    observed = json.loads(result.stdout)
    checks.append(dict(name='observed frame and draw', passed=observed['frame'] == 42 and observed['draws'] == 1))
    checks.append(dict(name='world translation and forward hit', passed=observed['origins'][0]['rays'][4]['distance'] == 5))
    checks.append(dict(name='two-sided reverse hit', passed=observed['origins'][1]['rays'][5]['distance'] == 1))
    checks.append(dict(name='outside triangle miss', passed='draw' not in observed['origins'][2]['rays'][4]))
    invalid = [
        ('truncated', data[:-1], '0 0 0 0'),
        ('limited', data[:-4] + struct.pack('<I', 1), '0 0 0 0'),
        ('bad-magic', bytes(4) + data[4:], '0 0 0 0'),
        ('empty-origins', data, ''),
        ('partial-origin', data, '0 1 2'),
        ('nonfinite-origin', data, '0 nan 0 0'),
        ('origin-bound', data, '0 0 0 0\n' * 65),
    ]
    for name, geometry, origins in invalid:
        checks.append(dict(name=name + ' rejected', passed=run(name, geometry, origins).returncode != 0))
    report = dict(status='PASS' if all(x['passed'] for x in checks) else 'FAIL', checks=checks)
    (args.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(status=report['status'], checks=len(checks))))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
