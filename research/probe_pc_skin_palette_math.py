"""Bounded original PC426B00 versus actual shared Skin palette implementation.

Only pristine guest instructions run; no game process or host native code is
loaded. Inputs are finite, ordinary affine matrices with fractional entries.
The shared host helper is not a full bit-identical x87 implementation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
from pc_instruction_emulator import PcInstructions, ROOT, run_bounded


def bits(values):
    return list(struct.unpack('<16I', struct.pack('<16f', *values)))


def specimens():
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    translated = identity.copy(); translated[12:15] = [5, 6, 7]
    scaled = [2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 1, 2, 3, 1]
    yield dict(name='known-noncommuting', left=bits(translated), right=bits(scaled))
    rng = random.Random(426_700)
    for i in range(32):
        pair = []
        for _ in range(2):
            values = [rng.uniform(-4, 4) for _ in range(16)]
            values[3] = values[7] = values[11] = 0; values[15] = 1
            pair.append(bits(values))
        yield dict(name=f'fractional-affine-{i:02}', left=pair[0], right=pair[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--require-exact', action='store_true')
    args = parser.parse_args(sys.argv[2:])
    if args.output.exists():
        raise ValueError('Preserve prior original/source comparison')
    source = ROOT / '.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSkinRenderTests.exe'
    cases = list(specimens()); p = PcInstructions()
    left, right, destination = [p.allocate(64) for _ in range(3)]
    executed = 0
    for case in cases:
        p.mu.mem_write(left, struct.pack('<16I', *case['left']))
        p.mu.mem_write(right, struct.pack('<16I', *case['right']))
        p.mu.mem_write(destination, b'\xcc' * 64)
        p.run(0x426B00, this=left, args=(destination, right))
        case['native'] = list(struct.unpack('<16I', bytes(p.mu.mem_read(destination, 64))))
        executed += sum(p.visits.values())
        if bytes(p.mu.mem_read(left, 64)) != struct.pack('<16I', *case['left']) or bytes(p.mu.mem_read(right, 64)) != struct.pack('<16I', *case['right']):
            raise AssertionError('Original function changed separate source matrices')
    stdin = str(len(cases)) + '\n' + '\n'.join(' '.join(map(str, c['left'] + c['right'])) for c in cases)
    result = subprocess.run([str(source), '--palette-math'], input=stdin, text=True, capture_output=True, check=True, timeout=10)
    host = json.loads(result.stdout)
    if len(host) != len(cases):
        raise AssertionError('Host specimen count changed')
    for case, observed in zip(cases, host):
        case.update(observed)
        for implementation in ('skin', 'shared'):
            case[implementation + 'DifferentCells'] = [i for i, (a, b) in enumerate(zip(case['native'], case[implementation])) if a != b]
    summary = {implementation: dict(exactCases=sum(not c[implementation+'DifferentCells'] for c in cases),
                                   differentCells=sum(len(c[implementation+'DifferentCells']) for c in cases))
               for implementation in ('skin', 'shared')}
    report = dict(schema=1, cases=cases, summary=summary, originalAddress='0x426B00', instructions=executed,
                  pristineSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
                  hostExeSha256=hashlib.sha256(source.read_bytes()).hexdigest().upper(),
                  sourceSha256=hashlib.sha256((ROOT/'Sparkplug/Code/Sparkplug/spSkin.cpp').read_bytes()).hexdigest().upper(),
                  helperSha256=hashlib.sha256((ROOT/'Sparkplug/Analysis/PC/spNodeTransformMath.h').read_bytes()).hexdigest().upper(),
                  scope='33 finite affine specimens; actual pristine PC426B00 guest instructions vs compiled spSkin and existing shared helper. No whole-game skin or arbitrary x87 bit-exact guarantee.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(summary=summary, instructions=executed)))
    return 1 if args.require_exact and summary['skin']['differentCells'] else 0


if __name__ == '__main__':
    raise SystemExit(main() if sys.argv[1:2] == ['--guest'] else run_bounded(Path(__file__), sys.argv[1:]))
