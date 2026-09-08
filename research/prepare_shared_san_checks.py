#!/usr/bin/env python3
"""Reuse sealed original-PC PRS evidence for the shared C# decoder/sampler.

No native rerun or corpus scan: rebuild the exact selected wire payloads and
verify every hash against CP127 before exposing them to the changed C# path.
"""
from pathlib import Path
import argparse
import hashlib
import json
from inspect_pc_san_keys import inspect
from probe_pc_san_vmd_keys import fixtures, vmd_fixture_bytes, ROOT


def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    directory = args.output.resolve()
    if not directory.is_relative_to(ROOT/'local-data/results'):
        raise ValueError('Local results only')
    validation = json.loads((ROOT/'research/tool-san-validation-2026-09-08.json').read_text(encoding='utf-8'))
    source = ROOT/validation['reports'][0]['path']
    assert digest(source) == validation['reports'][0]['sha256']
    native = json.loads(source.read_text(encoding='utf-8'))
    by_case = {name: channels for name, channels, _ in fixtures()}
    real = []
    for row in native['sources']:
        path = ROOT/row['path']
        assert digest(path) == row['sha256']
        _, tracks = inspect(path)
        for track in tracks:
            by_case[path.name+':'+track['name']] = {role: data['payload'] for role, data in track['roles'].items()}
        real.append({'path': str(path), 'sha256': row['sha256'], 'tracks': row['tracks']})
    directory.mkdir(parents=True, exist_ok=True)
    cases = []
    for index, row in enumerate(native['cases']):
        channels = by_case[row['case']]
        assert {str(role): hashlib.sha256(raw).hexdigest().upper() for role, raw in channels.items()} == row['channels']
        path = directory/f'case-{index:02d}.san'
        path.write_bytes(vmd_fixture_bytes(f'comparison-{index:02d}', channels))
        cases.append({'name': row['case'], 'path': str(path), 'sha256': digest(path),
                      'samples': [{'seconds': sample['seconds'], 'prs': sample['original'],
                                   'validity': sample['validity']} for sample in row['samples']]})
    report = {'original_report_path': str(source), 'original_report_sha256': digest(source),
              'generator_sha256': digest(Path(__file__)), 'cases': cases, 'real_sans': real}
    (directory/'input.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f'Prepared {len(cases)} exact-wire cases / {sum(len(row["samples"]) for row in cases)} original PRS samples, plus {len(real)} real SAN decodes.')


if __name__ == '__main__': main()
