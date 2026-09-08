#!/usr/bin/env python3
"""Small original-PC reference for a negative-time export endpoint regression."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys
import time
from pc_instruction_emulator import ROOT, PcInstructions, run_bounded
from probe_pc_animation_keys import install_acos_seams, euler
from probe_pc_san_vmd_keys import NativeTrack, wire, vmd_fixture_bytes


def guest(output):
    if not output.resolve().is_relative_to(ROOT/'local-data/results'):
        raise ValueError('Local results only')
    started = time.perf_counter()
    channels = {
        0: wire(1, (-1, 0), ((0, 0, 0), (10, 0, 0))),
        1: wire(1, (-1, 0), ((0, 0, 0, 1), euler((0, 0, .6)))),
        2: wire(1, (-1, 0), ((1, 1, 1), (1, 1, 1))),
    }
    directory = output.parent/'vmd-fixtures'
    directory.mkdir(parents=True, exist_ok=True)
    name = 'negative-two-key-endpoint'
    raw = vmd_fixture_bytes(name, channels)
    path = directory/(name+'.san')
    path.write_bytes(raw)
    machine = PcInstructions()
    install_acos_seams(machine)
    native = NativeTrack(machine, channels)
    # At the pre-boundary target float, retain the native pre-boundary value.
    # Source float32 nextafter(0,-inf) cannot survive a later +1 time shift.
    f32 = lambda value: struct.unpack('<f', struct.pack('<f', value))[0]
    guard = f32(1-f32(1/1500))
    pairs = [(0, -1), (.5, -.5), (f32(.999), f32(f32(.999)-1)), (guard, f32(guard-1)),
             (struct.unpack('<f', bytes.fromhex('ffff7f3f'))[0], -2**-149),
             (1, 0), (1.5, .5), (2, 1), (3, 2)]
    samples = []
    for target, source in pairs:
        prs, flags = native.sample(source)
        samples.append({'seconds': target, 'sourceSeconds': source,
            'receiverSubframeDiagnostic': source == -2**-149,
            'tracks': {'Pelvis': {str(role+2): value for role, value in enumerate(prs) if flags[role]},
                       'L_Bicep': {'3': prs[1]}}})
    assert abs(samples[4]['tracks']['Pelvis']['2'][0]-10) < 1e-6
    assert samples[5]['tracks']['Pelvis']['2'][0] == 0
    report = {'status': 'passed', 'kind': 'original-PC-negative-SAN-export-reference',
        'vmd_fixtures': [{'san': path.name, 'sha256': hashlib.sha256(raw).hexdigest().upper(),
            'duration': 2, 'timeOffset': 1, 'samples': samples}],
        'probeSha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        'pcSha256': hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        'elapsedSeconds': time.perf_counter()-started,
        'limits': ['Original reader 43DB90 and sampler 479290; same bounded stream/descriptor/CRT seams as CP127',
                   '100000 instructions/2s per native call; 64KiB arena; 30s separate process',
                   'Synthetic two-key channels, negative source start; export shifts timeline by +1s']}
    output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'PASS original PC negative-time endpoint: {len(samples)} poses, {report["elapsedSeconds"]:.3f}s')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--guest', action='store_true')
    args = parser.parse_args()
    if args.guest: guest(args.output)
    else: sys.exit(run_bounded(Path(__file__), ['--output', str(args.output)]))
