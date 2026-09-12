"""Read closed fixture captures; compare alpha visibility with system D3D9.

This is a narrow native/stock-Remix GPU contract check, not a claim that their
lighting models or final radiometric values are equivalent.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageStat


def read_run(path):
    path = path.resolve(strict=True)
    records = [json.loads(line) for line in (path / 'fixture.jsonl').read_text().splitlines()]
    if not records or records[-1].get('event') != 'complete' or records[-1].get('interrupted'):
        raise ValueError(f'Fixture did not complete: {path}')
    if any(r.get('event') == 'error' for r in records):
        raise ValueError(f'Fixture reports an error: {path}')
    captures = [r for r in records if r.get('event') == 'capture']
    if len(captures) != 16:
        raise ValueError(f'Expected 16 captures, got {len(captures)}: {path}')
    result = []
    for item in captures:
        file = path / item['file']
        if file.parent != path or file.suffix != '.bmp':
            raise ValueError('Unexpected capture name')
        with Image.open(file) as image:
            if image.size != (960, 540):
                raise ValueError('Unexpected fixture dimensions')
            cells = [ImageStat.Stat(image.crop((x-30, y-30, x+30, y+30)).convert('RGB')).mean
                     for y in (160, 380) for x in (192, 384, 576, 768)]
        result.append(dict(file=item['file'], frame=item['frame'], cells=cells,
                           sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
    return dict(path=str(path), captures=result, backend=next(r['backend'] for r in records if r.get('event') == 'layout'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--remix', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Use a fresh output path to preserve evidence')
    native, remix = read_run(args.native), read_run(args.remix)
    if native['backend'] != 'system' or remix['backend'] != 'stock-remix':
        raise ValueError('Backend mismatch')
    checks = []

    def check(name, condition):
        checks.append(dict(name=name, passed=bool(condition)))

    check('Fresh Remix buffers differ; a stale window image is rejected',
          len({c['sha256'] for c in remix['captures']}) >= 5)
    check('Native captures include changing alpha-test outcomes',
          len({c['sha256'] for c in native['captures']}) >= 2)
    for index in range(8, 16):
        left, right = native['captures'][index], remix['captures'][index]
        check(f'Case {index} identity matches', left['file'] == right['file'])
        # Native clear is RGB(8,8,8); debug albedo background is zero. The
        # controlled colored polygon is separated from both by these margins.
        native_visible = max(left['cells'][7]) > 16
        remix_visible = max(right['cells'][7]) > 2
        if index == 8:
            check('Reproduce raw disabled NEVER mismatch in stock Remix', native_visible and not remix_visible)
        else:
            check(f'Case {index} translated alpha visibility matches native D3D9', native_visible == remix_visible)
    check('Opaque API material exposes independent nonzero emission', max(remix['captures'][3]['cells'][7]) > 20)
    check('Legacy D3DMATERIAL9 emissive did not become surface emission in this fixture',
          max(remix['captures'][3]['cells'][2]) < 1)
    check('Explicit API emission survives disabling the legacy emissive blend override',
          max(abs(a-b) for a,b in zip(remix['captures'][3]['cells'][7], remix['captures'][6]['cells'][7])) < 1)
    report = dict(status='PASS' if all(c['passed'] for c in checks) else 'FAIL', checks=checks,
                  native=native, remix=remix,
                  scope='Alpha visibility and emission routing only; no final-lighting or game shader decomposition equivalence claim.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(status=report['status'], checks=len(checks), failed=[c['name'] for c in checks if not c['passed']])))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
