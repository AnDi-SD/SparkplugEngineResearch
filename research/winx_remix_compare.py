"""Own bounded live-config comparison using an unchanged native game camera.

Read-only process observations and existing window controls; no game patches.
Each case starts from explicit baseline settings. Restore baseline in finally:
deleting a live-config key alone does not reset its already applied value.
"""
import argparse
import datetime
import json
from pathlib import Path
import re
import subprocess
import sys
import time

from winx_remix_state import ROOT, StateReader


def camera(reader):
    snapshot = reader.camera_snapshot()
    main = next((x for x in snapshot['cameras'] if x['address'] == snapshot['mainCamera']), None)
    if main is None:
        raise RuntimeError('Main camera unavailable; comparison would be unverified')
    return {k: main[k] for k in ('address', 'view', 'projection', 'viewport')}


def control(pid, capture=None, settle=0):
    args = [sys.executable, '-B', str(ROOT / 'research/winx_remix_window.py'), '--pid', str(pid), '--settle', str(settle)]
    if capture:
        args += ['--capture', str(capture)]
    result = subprocess.run(args, capture_output=True, text=True, timeout=40)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    if not capture:
        # The window helper's --settle is scoped to capture. Live config also
        # needs rendering time when focusing/restoring without a screenshot.
        time.sleep(settle)


def stable_camera(reader):
    previous, since = camera(reader), time.monotonic()
    deadline = since + 15
    while time.monotonic() < deadline:
        time.sleep(.25)
        current = camera(reader)
        if current != previous:
            previous, since = current, time.monotonic()
        elif time.monotonic() - since >= 2:
            return current
    raise RuntimeError('Native camera did not remain identical for two seconds within the 15-second limit')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--name', required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', args.name):
        parser.error('Invalid comparison name')
    run = args.run.resolve()
    if not run.is_relative_to((ROOT / 'local-data/rtx-remix/runs').resolve()):
        parser.error('Only an existing isolated probe run is supported')
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    live = Path(launch['environment'].get('WINX_REMIX_LIVE_CONFIG') or '').resolve()
    if live != run / 'live.conf':
        parser.error('Run was not launched with its isolated live config')
    plan = json.loads(args.plan.read_text(encoding='utf-8-sig'))
    baseline, cases = plan['baseline'], plan['cases']
    if not 1 <= len(cases) <= 16 or not 1 <= len(baseline) <= 16:
        parser.error('Use 1..16 cases and baseline settings')
    settle = plan.get('settleSeconds', 5)
    if not 1 <= settle <= 15:
        parser.error('Settle must be 1..15 seconds')
    for settings in [baseline] + [case['settings'] for case in cases]:
        if not set(settings).issubset(baseline):
            parser.error('Every changed setting requires an explicit restore value in baseline')
        for key, value in settings.items():
            if not re.fullmatch(r'(?:rtx\.[A-Za-z0-9_.]+|winx\.(?:sceneLightGain|keepSceneGeometry))', key):
                parser.error('Unsupported live key')
            if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9_., +()\-]{1,128}', value):
                parser.error('Invalid live value')
    names = [case['name'] for case in cases]
    if len(set(names)) != len(names) or any(not re.fullmatch(r'[A-Za-z0-9_-]{1,48}', name) for name in names):
        parser.error('Use unique simple case names')
    folder = run / args.name
    folder.mkdir(exist_ok=False)
    original = live.read_text(encoding='utf-8-sig') if live.exists() else ''
    (folder / 'live.before').write_text(original, encoding='utf-8')
    (folder / 'plan.json').write_text(json.dumps(plan, indent=2), encoding='utf-8')
    # Preserve unrelated keys. Explicit baseline replaces existing tested keys.
    prefix = '\n'.join(line for line in original.splitlines() if line.split('=', 1)[0].strip() not in baseline)
    def apply(settings):
        content = prefix + '\n' + '\n'.join(f'{k} = {v}' for k, v in settings.items()) + '\n'
        temporary = live.with_suffix('.comparison-tmp')
        temporary.write_text(content, encoding='utf-8')
        temporary.replace(live)
    report = dict(pid=launch['pid'], started=datetime.datetime.now(datetime.timezone.utc).isoformat(), cases=[])
    reader = StateReader(launch['pid'])
    try:
        control(launch['pid'], settle=2)
        report['camera'] = reference = stable_camera(reader)
        report['state'] = reader.snapshot()
        if report['state'].get('active') == 2:
            raise RuntimeError('Gardenia 2 is excluded from current testing')
        for case in cases:
            settings = {**baseline, **case['settings']}
            before = camera(reader)
            apply(settings)
            path = folder / (case['name'] + '.png')
            control(launch['pid'], capture=path, settle=settle)
            after = camera(reader)
            result = dict(name=case['name'], settings=settings, before=before, after=after,
                          cameraMatched=before == after == reference,
                          state=reader.snapshot(), captured=datetime.datetime.now(datetime.timezone.utc).isoformat())
            result['levelMatched'] = result['state'].get('active') == report['state'].get('active')
            report['cases'].append(result)
            (folder / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
            print(json.dumps(dict(case=case['name'], cameraMatched=result['cameraMatched'])), flush=True)
            if not result['cameraMatched'] or not result['levelMatched']:
                raise RuntimeError('Camera or level changed; stop instead of treating images as a matched comparison')
    finally:
        apply(baseline)
        try:
            control(launch['pid'], settle=2)
            report['restoreCamera'] = camera(reader)
            report['baselineRestoredFile'] = True
        finally:
            reader.close()
            report['finished'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            (folder / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(str(folder))


if __name__ == '__main__':
    main()
