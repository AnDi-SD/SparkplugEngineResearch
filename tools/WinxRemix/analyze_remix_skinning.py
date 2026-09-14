"""Compare authored API skin silhouettes with baked-reference A/B/A captures.

The small triangle permits CPU skinning. The subdivided mesh exceeds the pinned
renderer's CPU threshold; branch attribution still depends on that source/binary.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageChops


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def mask(path, left=False):
    with Image.open(path) as image:
        if image.size != (960, 540):
            raise ValueError(f'Unexpected capture size: {path}')
        r, g, b = image.convert('RGB').split()
        result = ImageChops.darker(ImageChops.darker(r, g), b).point(lambda value: 255 if value >= 200 else 0)
    # Fixed D3D witness at left; stock welcome banner at top. Neither region
    # overlaps any declared projected triangle in this authored experiment.
    if left:
        ImageDraw.Draw(result).rectangle((0, 0, 224, 539), fill=0)
        ImageDraw.Draw(result).rectangle((476, 0, 959, 539), fill=0)
    else:
        ImageDraw.Draw(result).rectangle((0, 0, 479, 539), fill=0)
    ImageDraw.Draw(result).rectangle((0, 0, 959, 63), fill=0)
    return result


def compare(a, b):
    intersection = ImageChops.darker(a, b).histogram()[255]
    union = ImageChops.lighter(a, b).histogram()[255]
    return dict(firstPixels=a.histogram()[255], secondPixels=b.histogram()[255],
                intersection=intersection, union=union,
                iou=intersection / union if union else 0)


def analyze(run):
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    exit_info = json.loads((run / 'exit.json').read_text(encoding='utf-8-sig'))
    if exit_info['timeout'] or exit_info.get('memoryExceeded', False) or exit_info.get('systemPressure', False) or exit_info['exitCode'] != 0 or exit_info['pid'] != launch['pid']:
        raise ValueError('Fixture did not record a clean process exit')
    rows = [json.loads(line) for line in (run / 'fixture.jsonl').read_text().splitlines()]
    if not rows or rows[-1]['event'] != 'complete' or any(r['event'] == 'error' for r in rows):
        raise ValueError('Fixture journal is incomplete or contains API errors')
    maximum = launch['maximumBonesPerVertex']
    contract = next(r for r in rows if r['event'] == 'contract')
    signed_weights = bool(launch.get('signedWeights', False))
    if bool(contract.get('signedWeights', False)) != signed_weights:
        raise ValueError('Launched and observed weight encoding differ')
    half_resolution = bool(launch.get('halfResolution', False))
    configuration = {r['key']: r['value'] for r in rows if r['event'] == 'config'}
    if half_resolution and (configuration.get('rtx.upscalerType') != '2' or configuration.get('rtx.resolutionScale') != '0.5'):
        raise ValueError('Half-resolution run did not preserve the declared NIS profile')
    if contract.get('paired') is not True or contract.get('pixelSeparation') != 288:
        raise ValueError('Requires simultaneous-reference fixture; preserve earlier temporal-only verdicts')
    vertex_count = contract['vertices']
    subdivisions = contract.get('subdivisions', 1)
    if launch.get('subdivisions', 1) != subdivisions:
        raise ValueError('Launched and observed topology differ')
    if subdivisions not in (1, 24) or vertex_count != (subdivisions + 1) * (subdivisions + 2) // 2:
        raise ValueError('Unexpected fixture geometry size')
    indices = next((r['indices'] for r in rows if r['event'] == 'mesh_topology'), [0, 1, 2])
    if len(indices) != subdivisions * subdivisions * 3 or any(type(i) is not int or i < 0 or i >= vertex_count for i in indices):
        raise ValueError('Unexpected fixture topology')
    captured = [r['file'] for r in rows if r['event'] == 'capture']
    expected_names = [f'b{b}-p{p}-{mode}.bmp' for b in range(1, maximum + 1)
                      for p in range(2) for mode in ('reference', 'skin', 'reference-return')]
    if captured != expected_names or rows[-1]['captures'] != len(expected_names):
        raise ValueError('Capture sequence is not the declared A/B/A sequence')
    poses = {(r['bonesPerVertex'], r['pose']): r for r in rows if r['event'] == 'pose_input'}
    results = []
    for bones in range(1, maximum + 1):
        for pose in range(2):
            prefix = f'b{bones}-p{pose}'
            reference, skin, returned = [mask(run / f'{prefix}-{mode}.bmp')
                                        for mode in ('reference', 'skin', 'reference-return')]
            expected = Image.new('L', (960, 540))
            points = [(480 + (x + 1.5) * 480 / z, 270 - (y - .2) * 480 / z)
                      for x, y, z in poses[bones, pose]['expectedPositions']]
            if len(points) != vertex_count:
                raise ValueError('Missing authored reference vertices')
            painter = ImageDraw.Draw(expected)
            for face in range(0, len(indices), 3):
                painter.polygon([points[i] for i in indices[face:face+3]], fill=255)
            stability = compare(reference, returned)
            deformation = compare(reference, skin)
            projection = compare(reference, expected)
            paired = {}
            for mode, right_mask in zip(('reference', 'skin', 'reference-return'), (reference, skin, returned)):
                left_mask = ImageChops.offset(mask(run / f'{prefix}-{mode}.bmp', left=True), 288, 0)
                paired[mode] = compare(left_mask, right_mask)
            # Two instances in one present have identical subpixel jitter.
            # Temporal A/A drift remains reported, but is not the skin oracle.
            baseline_ok = projection['iou'] >= .90 and all(paired[mode]['iou'] >= .98 and min(paired[mode]['firstPixels'], paired[mode]['secondPixels']) > 100 for mode in ('reference', 'reference-return'))
            results.append(dict(bonesPerVertex=bones, pose=pose, projectedVertices=points,
                                referenceStable=baseline_ok, referenceABA=stability,
                                referenceVsProjectedTriangle=projection,
                                skinVsReference=deformation,
                                simultaneousReference=paired,
                                status='PASS' if baseline_ok and paired['skin']['iou'] >= .97 else 'FAIL'))
    pose_changes = []
    for bones in range(1, maximum + 1):
        difference = compare(mask(run / f'b{bones}-p0-reference.bmp'), mask(run / f'b{bones}-p1-reference.bmp'))
        pose_changes.append(dict(bonesPerVertex=bones, referencePoseIoU=difference['iou'], distinct=difference['iou'] < .90))
    success = all(r['status'] == 'PASS' for r in results) and all(p['distinct'] for p in pose_changes)
    files = ['launch.json', 'exit.json', 'build.json', 'fixture.jsonl', *expected_names]
    return dict(schema=1, status='PASS' if success else 'FAIL', run=str(run), results=results,
                vertices=vertex_count, subdivisions=subdivisions,
                halfResolution=half_resolution, signedWeights=signed_weights,
                poseChanges=pose_changes,
                scope=f'Authored {vertex_count}-vertex mesh, '+('negative/nonunit weights encoded through signed palettes and a zero final influence; shared Fixed reference' if signed_weights else 'normalized varying weights')+'. Four original rigid bones, two palettes, immutable skin mesh across poses and independent world translation. Each screenshot contains a simultaneous reference offset by288 pixels. Temporal jitter is reported separately. The small mesh permits CPU skinning; 325 vertices exceed the pinned renderer threshold256. This threshold inference is not GPU command tracing. No game skin/material or temporal motion-vector coverage is claimed.',
                thresholds=dict(simultaneousControl=.98, expectedProjection=.90, simultaneousSkinReference=.97, minimumPixels=100),
                hashes={name: digest(run / name) for name in files})


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Preserve prior verdict; choose a fresh output path')
    report = analyze(args.run.resolve())
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(status=report['status'], signedWeights=report['signedWeights'], cases=len(report['results']),
                         minimumSkinIoU=min(r['simultaneousReference']['skin']['iou'] for r in report['results']),
                         poseChanges=report['poseChanges'])))
    raise SystemExit(0 if report['status'] == 'PASS' else 1)
