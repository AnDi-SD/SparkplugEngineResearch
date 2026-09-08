"""Bounded original-PC evidence for Viewer V-02; repeated linear key times."""
from pathlib import Path
import hashlib
import json
import math
import struct
import sys
from pc_instruction_emulator import PcInstructions, ROOT, run_bounded
from probe_pc_animation_keys import install_acos_seams
from probe_pc_san_vmd_keys import NativeTrack, vmd_fixture_bytes


def guest():
    output = ROOT/'local-data/results/viewer-core-audit-20260908/equal-times'
    output.mkdir(parents=True, exist_ok=True)
    machine = PcInstructions()
    install_acos_seams(machine)
    cases = []
    for name, times in [('two-equal', [0, 0]), ('three-equal-start', [0, 0, 1]), ('three-equal-end', [0, 1, 1])]:
        values = [float(i*3+c) for i in range(len(times)) for c in range(3)]
        payload = struct.pack('<II', 1, len(times)) + struct.pack('<'+'f'*(len(times)+len(values)), *times, *values)
        channels = {0: payload, 1: struct.pack('<II5f', 1, 1, 0, 0, 0, 0, 1)}
        native = NativeTrack(machine, channels)
        path = output/(name+'.san')
        raw = vmd_fixture_bytes(name, channels)
        path.write_bytes(raw)
        samples = []
        for seconds in [-1, 0, .25, 1, 2]:
            prs, flags = native.sample(seconds)
            samples.append({'seconds': seconds, 'position': list(prs[0]), 'validity': list(flags),
                            'finite': all(math.isfinite(v) for v in prs[0])})
        cases.append({'name': name, 'path': path.relative_to(ROOT).as_posix(),
                      'sha256': hashlib.sha256(raw).hexdigest().upper(), 'times': times, 'samples': samples})
    report = {'reader': '43DB90', 'sampler': '479290', 'cases': cases,
              'exeSha256': hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
              'limits': '64KiB guest arena; 100000 instructions/2s per call; 30s outer; existing stream/pool/CRT seams',
              'scope': 'Three synthetic linear position channels. No general malformed SAN validity claim.'}
    (output/'original.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    if '--guest' in sys.argv: guest()
    else: raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
