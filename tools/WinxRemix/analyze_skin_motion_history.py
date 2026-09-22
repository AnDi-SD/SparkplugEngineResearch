"""Compare GPU Skin motion with an independent rigid API control in the same frame.

The authored B2 profiles use positive and signed source weights. Interior pixels
check horizontal/vertical motion and return to rest; this is an 8-bit diagnostic
image comparison, not a readback of the GPU's floating-point history buffers.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageStat


def analyze(run):
    def read(name):
        return json.loads((run / name).read_text(encoding='utf-8-sig'))
    launch, execution, cleanup = read('launch.json'), read('exit.json'), read('cleanup.json')
    if (launch.get('motionHistory') is not True or launch.get('subdivisions') != 24
            or launch.get('maximumBonesPerVertex') != 2 or launch.get('halfResolution')
            or launch.get('signedWeights')):
        raise ValueError('Requires the full-resolution MotionHistory fixture with both source-weight profiles')
    if (execution['pid'] != launch['pid'] or execution['exitCode'] != 0
            or execution['timeout'] or execution['memoryExceeded'] or execution['systemPressure']):
        raise ValueError('The owned fixture did not complete within its resource profile')
    if (cleanup['remaining'] or cleanup['cleanupFailed'] or cleanup['clientExitCode'] != 0
            or any(not child['normalExit'] for child in cleanup['servers'])):
        raise ValueError('The operation did not finish cleanly')
    if 'motionKind' in launch:
        if (launch['motionKind'] != 'Translation' or not cleanup['servers']
                or any(child['exitCode'] != 0 for child in cleanup['servers'])):
            raise ValueError('Requires Translation with a retained, successful bridge exit')
    rows = [json.loads(line) for line in (run / 'fixture.jsonl').read_text().splitlines()]
    if (not rows or rows[-1].get('event') != 'complete' or rows[-1].get('captures') != 10
            or rows[-1].get('motionHistoryExperiment') is not True
            or any(row['event'] == 'error' for row in rows)):
        raise ValueError('Missing completion or an API error was recorded')
    if 'motionKind' in launch:
        completion = [row for row in rows if row['event'] == 'deviceCompletion']
        if (len(completion) != 1 or completion[0]['polls'] < 1 or completion[0]['milliseconds'] > 10000):
            raise ValueError('Missing bounded device completion after the queued commands')
    contract = next(row for row in rows if row['event'] == 'motion_contract')
    if (contract['vertices'] != 325 or contract['subdivisions'] != 24
            or contract['separationPixels'] != 288 or contract['skinCategoryIgnoresMotion']
            or contract['deltaPixels'] != [.24, .18]):
        raise ValueError('Unexpected motion contract')
    configuration = {row['key']: row['value'] for row in rows if row['event'] == 'config'}
    for key, value in {'rtx.upscalerType': '0', 'rtx.resolutionScale': '1',
                       'rtx.forceCameraJitter': 'False', 'rtx.postfx.enableMotionBlur': 'False'}.items():
        if configuration.get(key) != value:
            raise ValueError(f'Unexpected observed configuration: {key}')
    phases = [row for row in rows if row['event'] == 'motion_phase']
    expected_phases = [(signed, phase) for signed in range(2) for phase in range(5)]
    names = [f'motion-s{signed}-p{phase}.bmp' for signed, phase in expected_phases]
    if ([(row['signedProfile'], row['phase']) for row in phases] != expected_phases
            or [row['file'] for row in phases] != names
            or [row['file'] for row in rows if row['event'] == 'capture'] != names):
        raise ValueError('Missing or reordered phases/captures')
    results = []
    for phase in phases:
        with Image.open(run / phase['file']) as image:
            if image.size != (960, 540):
                raise ValueError('Unexpected capture size')
            image = image.convert('RGB')
            # Interior of each authored triangle. Fixed D3D witness and Remix
            # welcome banner do not overlap either 24x24 region.
            reference = image.crop((324, 265, 348, 289))
            skin = image.crop((612, 265, 636, 289))
        means = [ImageStat.Stat(im).mean for im in (reference, skin)]
        extrema = [im.getextrema() for im in (reference, skin)]
        difference = ImageChops.difference(reference, skin)
        mean_difference = ImageStat.Stat(difference).mean
        maximum_difference = [pair[1] for pair in difference.getextrema()]
        checks = {'pairedDifference': max(mean_difference) <= 2 and max(maximum_difference) <= 4,
                  'settledFrames': phase['frames'] >= 90}
        if phase['phase'] == 0:
            checks['bothAlbedoControlsVisible'] = all(min(ch[0] for ch in extent) >= 200 for extent in extrema)
        elif phase['phase'] in (1, 4):
            checks['stationaryReturnsToZero'] = all(max(ch[1] for ch in extent) <= 3 for extent in extrema)
        else:
            # Debug21 encodes abs(pixel motion) directly: 0.24*255 ->61,
            # 0.18*255 ->46. The independent rigid control must also agree.
            expected = [61, 46 if phase['phase'] == 3 else 0, 0]
            checks['expectedRigidMotion'] = all(abs(a-b) <= 2 for a, b in zip(means[0], expected))
            checks['expectedSkinMotion'] = all(abs(a-b) <= 2 for a, b in zip(means[1], expected))
            checks['unusedChannelsZero'] = all(extent[2][1] <= 3 for extent in extrema)
            if phase['phase'] == 2:
                checks['horizontalOnly'] = all(extent[1][1] <= 3 for extent in extrema)
        results.append(dict(**phase, referenceMean=means[0], skinMean=means[1],
                            meanAbsoluteDifference=mean_difference, maximumAbsoluteDifference=maximum_difference,
                            checks=checks, passed=all(checks.values())))
    files = ['launch.json', 'exit.json', 'cleanup.json', 'build.json', 'fixture.jsonl', *names]
    return dict(schema=1, status='PASS' if all(row['passed'] for row in results) else 'FAIL',
                run=str(run), contract=contract, phases=results, completion=rows[-1], cleanup=cleanup,
                rendererSha256=launch['rendererSha256'],
                hashes={name: hashlib.sha256((run / name).read_bytes()).hexdigest().upper() for name in files},
                scope='Authored B2 positive/signed source weights,325 vertices,simultaneous rigid API control. '
                      'Interior 8-bit RGB motion-debug comparison;325 vertices exceed the pinned renderer CPU '
                      'threshold256,which is a source-based inference rather than command tracing. '
                      'Does not qualify rotations,nonrigid deformation,camera movement,DLSS/TAA or game-wide history.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Preserve the prior verdict; choose a fresh output path')
    report = analyze(args.run.resolve())
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(status=report['status'], phases=len(report['phases']),
                         maximumPairedDifference=max(max(row['maximumAbsoluteDifference']) for row in report['phases']))))
    raise SystemExit(0 if report['status'] == 'PASS' else 1)
