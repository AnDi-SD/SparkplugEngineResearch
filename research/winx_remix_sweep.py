"""Own GPU smoke-test driver: verified F1 selection, read-only readiness, screenshots.

Uses the existing game/debug executable and ordinary keyboard input. No writes
to process memory and no game-code changes. Loading is not a full playthrough.
"""
import argparse
import datetime
import json
from pathlib import Path
import subprocess
import sys
import time

from winx_remix_state import ROOT, StateReader


def catalog():
    # Reuse the maintained native test catalog rather than a second name table.
    import re
    text = (ROOT / 'tools/SmoViewer/SmoNativeValidator.Core/WinxClubLevelCatalog.cs').read_text(encoding='utf-8')
    blocks = re.findall(r'string\[\] (?:names|challengeNames)\s*=\s*\[(.*?)\];', text, re.S)
    if len(blocks) != 2:
        raise RuntimeError('Native test catalog format changed')
    main, challenges = [re.findall(r'"([A-Za-z0-9_]+)"', block) for block in blocks]
    if len(main) != 37 or len(challenges) != 9:
        raise RuntimeError('Unexpected native catalog coverage')
    return {**dict(enumerate(main, 1)), **dict(enumerate(challenges, 41))}


class Sweep:
    def __init__(self, pid, run):
        self.reader = StateReader(pid)
        self.pid, self.run = pid, run
        self.levels = catalog()

    def control(self, key=None, hold=.08, count=1, capture=None, settle=0):
        args = [sys.executable, '-B', str(ROOT / 'research/winx_remix_window.py'), '--pid', str(self.pid)]
        if key:
            args += ['--key', key, '--hold', str(hold), '--key-count', str(count), '--key-gap', '.09']
        if capture:
            args += ['--capture', str(capture), '--settle', str(settle)]
        done = subprocess.run(args, capture_output=True, text=True, timeout=40)
        if done.returncode:
            raise RuntimeError(done.stderr.strip() or done.stdout.strip())

    def set_debug_open(self, wanted):
        # F1 can miss a short press during a slow Remix frame. Inspect native
        # menu state after each bounded press before sending any menu action.
        for _ in range(3):
            state = self.reader.snapshot()
            if bool(state.get('debugOpen')) == wanted:
                return state
            self.control('f1', hold=.8)
        state = self.reader.snapshot()
        if bool(state.get('debugOpen')) != wanted:
            raise RuntimeError('Could not confirm debug menu open/closed state')
        return state

    def select(self, level, restart=0):
        if restart > 3:
            raise RuntimeError('Gameplay repeatedly interrupted F1 selection')
        if not 1 <= level <= 37:
            raise ValueError('F1 directly supports only 1..37; use startup mode for challenges')
        state = self.reader.snapshot()
        if state.get('active') in (64, 70, 71):
            if state.get('debugOpen'):
                self.set_debug_open(False)
            for _ in range(32):
                if self.reader.snapshot().get('active') not in (64, 70, 71):
                    break
                # Dialogue acknowledgement is edge-triggered, and a short
                # pulse can fall entirely between slow Remix frames. The level
                # submenu keeps its separate single-frame Enter below.
                self.control('enter', hold=.25)
            state = self.reader.snapshot()
            if state.get('active') in (64, 70, 71):
                raise RuntimeError('Dialogue still owns input; refusing level navigation')
        if state.get('debugOpen') and len(state['menus']) != 2:
            state = self.set_debug_open(False)
        if not state.get('debugOpen'):
            self.set_debug_open(True)
        for attempt in range(5):
            state = self.reader.snapshot()
            if len(state['menus']) == 2 and state['menus'][-1]['count'] == 37:
                break
            if len(state['menus']) != 1 or state['menus'][0]['selected'] != 0:
                raise RuntimeError(f'Unexpected root selection: {state}')
            self.control('enter', hold=.001)
        else:
            raise RuntimeError('Could not open level selector with single key press')
        for attempt in range(12):
            state = self.reader.snapshot()
            if state.get('active') in (64, 70, 71):
                return self.select(level, restart + 1)
            if len(state['menus']) != 2 or state['menus'][-1]['count'] != 37:
                raise RuntimeError('Level selector changed during navigation')
            delta = level - 1 - state['menus'][-1]['selected']
            print(json.dumps(dict(navigate=level, attempt=attempt, selected=state['menus'][-1]['selected'], active=state.get('active'))), flush=True)
            if delta == 0:
                return state
            # Debug selection polls held arrows. In fast UI/death screens an
            # .08-second hold crosses many rows; .001 misses most input polls.
            # Use a shorter pulse and confirm the actual index before activation.
            self.control('down' if delta > 0 else 'up', hold=.01, count=abs(delta))
        raise RuntimeError('Could not confirm requested level selection')

    def shader_snapshot(self):
        path = self.run / 'shaders-client/audit.jsonl'
        with path.open('rb') as f:
            f.seek(max(0, path.stat().st_size - 131072))
            lines = f.read().splitlines()
        for line in reversed(lines):
            try:
                event = json.loads(line)
                if event.get('event') == 'snapshot':
                    return event
            except (ValueError, UnicodeError):
                pass
        return None

    def test(self, level, startup=False):
        folder = self.run / 'levels' / f'{level:02}-{self.levels[level]}'
        if folder.exists():
            attempt = 2
            while folder.with_name(folder.name + f'-attempt-{attempt}').exists():
                attempt += 1
            folder = folder.with_name(folder.name + f'-attempt-{attempt}')
        folder.mkdir(parents=True, exist_ok=False)
        record = dict(level=level, name=self.levels[level], pid=self.pid,
                      started=datetime.datetime.now().astimezone().isoformat(), route='startLevel' if startup else 'F1',
                      status='running', states=[], directory=str(folder.relative_to(ROOT)))
        target = folder / 'result.json'
        def save():
            target.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
        save()
        try:
            if not startup:
                record['selection'] = self.select(level)
                self.control(capture=folder / 'selection.png')
                self.control('enter', hold=.001)
            begin, stable = time.monotonic(), None
            last = None
            while time.monotonic() - begin < 150:
                state = self.reader.snapshot()
                reduced = {k: state.get(k) for k in ('current', 'active', 'pending', 'stack', 'stateStack')}
                if reduced != last:
                    record['states'].append(dict(seconds=round(time.monotonic()-begin, 2), **reduced))
                    last = reduced
                ready = level in state.get('stateStack', []) and state.get('pending') == 0
                stable = (stable or time.monotonic()) if ready else None
                if stable and time.monotonic() - stable >= 4:
                    break
                time.sleep(.5)
            else:
                record['status'] = 'load_timeout'
                self.control(capture=folder / 'timeout.png')
                save()
                print(json.dumps(record), flush=True)
                return False
            record['readySeconds'] = round(time.monotonic() - begin, 2)
            state = self.reader.snapshot()
            if state.get('debugOpen'):
                self.set_debug_open(False)
            # State 71 is the tutorial dialog, visually verified in Gardenia02.
            # Preserve it, then acknowledge using its displayed Enter control.
            if self.reader.snapshot().get('active') == 71:
                self.control(capture=folder / 'tutorial.png')
                for _ in range(8):
                    if self.reader.snapshot().get('active') != 71:
                        break
                    self.control('enter', hold=.05)
            self.control(capture=folder / 'idle.png', settle=3)
            record['idleState'] = self.reader.snapshot()
            record['idleShaderSnapshot'] = self.shader_snapshot()
            self.control('right', hold=.25, capture=folder / 'moved.png', settle=3)
            record['movedState'] = self.reader.snapshot()
            record['movedShaderSnapshot'] = self.shader_snapshot()
            idle, moved = record['idleShaderSnapshot'], record['movedShaderSnapshot']
            record['framesAdvanced'] = moved['frame'] - idle['frame'] if idle and moved else None
            record['status'] = 'loaded_and_presenting' if record['framesAdvanced'] and record['framesAdvanced'] > 0 else 'loaded_no_frame_progress'
            record['finished'] = datetime.datetime.now().astimezone().isoformat()
            save()
            print(json.dumps({k: record[k] for k in ('level', 'name', 'status', 'readySeconds', 'framesAdvanced', 'directory')}), flush=True)
            return record['status'] == 'loaded_and_presenting'
        except Exception as error:
            record.update(status='test_interrupted', error=str(error))
            save()
            print(json.dumps(record), flush=True)
            raise


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', required=True, type=int)
    parser.add_argument('--run', required=True, type=Path)
    parser.add_argument('--levels', required=True, type=int, nargs='+')
    parser.add_argument('--startup', action='store_true')
    args = parser.parse_args()
    run = args.run.resolve()
    expected_root = ROOT / 'local-data/rtx-remix/runs'
    if not run.is_relative_to(expected_root) or not (run / 'launch.json').is_file():
        parser.error('Use an existing Start-Probe run inside the workspace')
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    if launch['pid'] != args.pid or any(i not in catalog() for i in args.levels):
        parser.error('PID or level does not match the owned run/catalog')
    if args.startup and len(args.levels) != 1:
        parser.error('Startup mode accepts exactly one level')
    sweep = Sweep(args.pid, run)
    try:
        for level in args.levels:
            if not sweep.test(level, args.startup):
                sys.exit(2)
    finally:
        sweep.reader.close()
