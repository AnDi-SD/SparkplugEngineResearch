#!/usr/bin/env python3
"""Source whole SAN load/save -> original whole FFPS reader, one fresh child.

The full-file producer is explicitly host orchestration of reconstructed
contracts, not evidence for a located native SaveResources function.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import time

from pc_instruction_emulator import ROOT, run_bounded
from pc_loader_fixtures import PCFileBytesFixture, empty_manager, empty_fat, animation_rtti
from pc_stl_fixtures import install_char_traits
from inspect_pc_san_keys import DEFAULT, inspect, u32
import probe_pc_san_loader as original
from compare_pc_san_reader import compare

SOURCE = ROOT / '.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSanReaderTests.exe'
OUTPUT = ROOT / 'local-data/results/cycle-20260908-0700'


def compiled(mode, path):
    result = subprocess.run([str(SOURCE), mode, str(path)], cwd=ROOT,
                            capture_output=True, timeout=10, check=True)
    if len(result.stdout) > 1_000_000:
        raise ValueError('Bounded source capture exceeded')
    return result.stdout.decode('utf-8')


def emit(name, *, lane='native'):
    text = compiled('--rewrite-file', DEFAULT / name)
    raw = bytes.fromhex(next(line.split(' ', 1)[1] for line in text.splitlines()
                            if line.startswith('SAN_FILE_HEX ')))
    if not 36 <= len(raw) <= 0x10000:
        raise ValueError('Whole SAN native witness limited to 64 KiB file bytes')
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / (Path(name).stem + '-' + lane + '-source.san')
    path.write_bytes(raw)
    return path, raw


def portable_corpus():
    total = 0
    for name in ('bbush.san', 'bflower.san', 'barrel.san', 'bw.san'):
        path, raw = emit(name, lane='portable')
        expected = json.loads(compiled('--inspect-file', DEFAULT / name))
        observed = json.loads(compiled('--inspect-file', path))
        expected.pop('usedPools'); observed.pop('usedPools')
        capacity_before = expected.pop('capacity'); capacity_after = observed.pop('capacity')
        # Original 43DFE0 always writes field64=logical track count. Its reader
        # reserves count+1. Barrel omits field64 on disk and initially has N,
        # so native canonical serialization changes reserve capacity to N+1.
        assert capacity_after == len(expected['tracks']) + 1
        assert capacity_before == capacity_after or (
            name == 'barrel.san' and capacity_before == len(expected['tracks']))
        total += compare(expected, observed)
        print('PORTABLE_FILE', name, len(raw), hashlib.sha256(raw).hexdigest().upper(),
              'capacity', capacity_before, '->', capacity_after, flush=True)
    print(f'PASS 4/4 portable whole-file load/save/reload cases; {total} equal values plus explicit native capacity canonicalization; not original corpus execution')
    return 0


def main():
    start = time.monotonic()
    path, raw = emit('bbush.san')
    reference = (DEFAULT / 'bbush.san').read_bytes()
    expected = json.loads(compiled('--inspect-file', DEFAULT / 'bbush.san'))
    source_reloaded = json.loads(compiled('--inspect-file', path))
    expected.pop('usedPools'); source_reloaded.pop('usedPools')
    source_count = compare(expected, source_reloaded)
    f = PCFileBytesFixture(raw); p = f.p
    animation_rtti(f); install_char_traits(p)
    del p.seams[0x454370]; del p.seams[0x453b10]
    names = f.call(0x454640)
    fat = empty_fat(f); manager, _ = empty_manager(f)
    p.put_uint(manager + 0x28, fat); p.put_uint(0x75dde8, manager)
    serializer = f.call(0x43dab0)
    f.call(0x422d90, this=manager, args=(0x56ee563a, serializer, 0xff, 1))
    before = original.checks
    animation = f.call(0x422b50, this=manager, args=(f.stream,))
    instructions = sum(p.visits.values()); visited = set(p.visits)
    original.check(animation in f.allocations and p.uint(animation) == 0x6de6cc, 'original Animation factory')
    for entry in (0x422260, 0x466b90, 0x465cd0, 0x4aa430, 0x4aab80, 0x422940,
                  0x4143f0, 0x414420, 0x467550, 0x43ecc0, 0x466760, 0x466870):
        original.check(entry in visited, f'actual whole-file stage {entry:08X}')
    original.check(f.position == len(raw) and not f.errors, 'source file consumed without diagnostics')
    original.check(p.uint(f.stream + 0x14) == u32(raw, 20), 'original data origin equals source header')
    original.check(all(op[2] == op[3] for op in f.io if op[0] == 'read'), 'no successful short reads')
    original.check(p.uint(fat + 0x28) == p.uint(fat + 0x34) == p.uint(fat + 0x50) == 0,
                   'original clears both FAT maps and insertion list')
    summary, tracks = inspect(DEFAULT / 'bbush.san')
    observed = original.capture(f, animation, summary, tracks, raw)
    observed.pop('usedPools')
    count = compare(expected, observed)
    f.call(p.uint(p.uint(animation)), this=animation, args=(1,))
    f.call(0x4228a0, this=manager)
    for address in (0x75db78, 0x75526c):
        obj = p.uint(address)
        if obj: f.call(p.uint(p.uint(obj)), this=obj, args=(1,))
    f.call(0x4545d0, this=names, args=(1,))
    original.check(set(f.allocations) == set(f.freed), 'all original allocations released')
    report = {
        'kind': 'source-file-original-reader-comparison', 'case': 'bbush.san',
        'sourceInputSha256': hashlib.sha256(reference).hexdigest().upper(),
        'sourceOutputSha256': hashlib.sha256(raw).hexdigest().upper(),
        'sourceOutputBytes': len(raw), 'nativeWholeReader': '0x00422B50',
        'wholeLoadInstructions': instructions, 'nativeAssertions': original.checks - before,
        'exactValues': count, 'portableReloadValues': source_count,
        'releasedAllocations': len(f.allocations), 'arenaReservedBytes': p.allocated,
        'arenaLimitBytes': p.arena_size, 'elapsedSeconds': round(time.monotonic() - start, 3),
        'sourceExecutableSha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest().upper(),
        'wholeWriterProvenance': 'host composition; no original whole Save function claimed',
    }
    (OUTPUT / 'cp99-san-file-roundtrip.json').write_text(
        json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('CAPTURE', json.dumps(report, sort_keys=True), flush=True)
    print(f'PASS 1/1 source-written complete SAN accepted by original reader: {count} exact values')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(portable_corpus() if '--portable-corpus' in sys.argv else main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
