"""Compare original visibility selections, D3D draws and a Remix camera recording.

Own diagnostic analysis. Camera matrices are compared as float32 bytes. A camera
sequence has no game frame IDs; matches prove accepted values, not frame latency.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import struct


def records(path):
    with path.open(encoding='utf-8-sig') as stream:
        for line in stream:
            # Live logs may end halfway through the final record.
            if not line.endswith('\n'):
                break
            yield json.loads(line)


def signature(camera):
    values = camera['view'] + camera['projection']
    return struct.pack('<32f', *values) if all(v is not None for v in values) else None


def analyze(run):
    result = dict(run=str(run), matrixComparison='bit-exact float32',
                  scope='sampled calls/draws; no claim about unsampled frames or Remix internal geometry')
    scene_path = run / 'scene-audit.jsonl'
    selections = defaultdict(list)
    if scene_path.exists():
        for row in records(scene_path):
            if row['event'] == 'visibility':
                selections[row['frame']].append(row)
        main = [r for rows in selections.values() for r in rows if r['isMain']]
        other = [r for rows in selections.values() for r in rows if not r['isMain']]
        objects = [set(s['address'] for s in r['supports'] if s) for r in main]
        result['visibility'] = dict(
            mainSelections=len(main), otherSelections=len(other),
            mainCountRange=[min(r['count'] for r in main), max(r['count'] for r in main)] if main else None,
            otherCountRange=[min(r['count'] for r in other), max(r['count'] for r in other)] if other else None,
            invalidVectors=sum(not r['vectorValid'] for rows in selections.values() for r in rows),
            nullSlots=sum(s is None for rows in selections.values() for r in rows for s in r['supports']),
            supportReadErrors=sum(bool(s and s.get('readError')) for rows in selections.values() for r in rows for s in r['supports']),
            uniqueMainViews=len({signature(r['camera']) for r in main if r['camera']}),
            uniqueMainSupports=len(set().union(*objects)),
            mainSelectionTransitions=sum(a != b for a, b in zip(objects, objects[1:])),
            # Demonstrates why the asynchronous final vector cannot be called main.
            framesEndingWithOtherCamera=sum(not rows[-1]['isMain'] and any(r['isMain'] for r in rows) for rows in selections.values()))
        registries = [r['sceneLights'] for r in main if r.get('sceneLights')]
        if registries:
            distant = defaultdict(set)
            types = set()
            for registry in registries:
                types.add(tuple(sorted(Counter(light['type'] for light in registry['lights']).items())))
                for light in registry['lights']:
                    if light['type'] == 0:
                        # Position does not affect a directional light. Retain
                        # the actual color/intensity and hierarchy-active flag.
                        distant[light['address']].add(json.dumps([
                            light['direction'], light['rgba'], light['intensity'],
                            light['enabled'], bool(light['flags'] & 0x100)]))
            result['sceneLights'] = dict(snapshots=len(registries),
                invalidRegistries=sum(not r['valid'] for r in registries),
                observedTypeCounts=[dict(items) for items in sorted(types)],
                directionalStateVariants={str(k): len(v) for k, v in distant.items()},
                scope='original scene membership and light state; this analyzer does not verify API submission/lifetime')
    sequence = run / 'workdir/camera-sequence.bin'
    accepted = set()
    if sequence.exists():
        with sequence.open('rb') as stream:
            header = stream.read(256)
            if len(header) != 256:
                raise ValueError('Truncated camera sequence header')
            count = struct.unpack_from('<I', header)[0]
            if count > 262144 or sequence.stat().st_size != 256 + count*1024:
                raise ValueError('Invalid or oversized camera sequence')
            for _ in range(count):
                accepted.add(stream.read(1024)[:128])
        result['remixSequence'] = dict(frames=count, uniqueViews=len(accepted),
                                      warning='matrix identity only; the format does not carry game frame IDs')
    counts = Counter()
    matched_views, matched_frames = set(), set()
    native_views, native_frames = set(), set()
    mode, interval = 0, []
    for draw in records(run / 'draws.jsonl'):
        if draw['event'] == 'live_config' and draw['key'] == 'rtx.cameraSequence.mode' and draw['result'] == 0:
            mode = int(draw['value'])
            interval.append([draw['frame'], mode])
        if draw['event'] != 'draw' or draw['projection'][11] == 0:
            continue
        raw = signature(draw)
        if mode == 1 and accepted:
            counts['sequenceWindowPerspectiveDraws'] += 1
            if raw in accepted:
                counts['sequenceWindowExactDraws'] += 1
                matched_views.add(raw)
                matched_frames.add(draw['frame'])
            else:
                counts['sequenceWindowUnmatchedDraws'] += 1
        rows = selections.get(draw['frame'], [])
        # A selection must precede this draw. The main selection can remain
        # relevant after a nested pass; matrix equality establishes the relation.
        cameras = {signature(r['camera']) for r in rows if r['isMain'] and r['camera'] and r['afterDraw'] < draw['draw']}
        if not cameras or not draw['z']:
            continue
        counts['worldDrawsWithMainSelection'] += 1
        if raw in cameras:
            counts['worldDrawsMatchingMain'] += 1
            native_views.add(raw)
            native_frames.add(draw['frame'])
        else:
            counts['worldDrawsDifferentFromMain'] += 1
    result['drawComparison'] = dict(counts)
    result['drawComparison'].update(nativeExactViews=len(native_views), nativeExactFrames=len(native_frames),
                                    sequenceExactViews=len(matched_views), sequenceExactFrames=len(matched_frames),
                                    sequenceModeChanges=interval)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = analyze(args.run)
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    print(text)
