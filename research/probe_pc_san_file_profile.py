#!/usr/bin/env python3
"""Fresh whole-SAN experiments under the explicit file execution profile.

Historical 100k micro stops are not resumed or reclassified. The 1M/8s profile
is selected before guest construction; no additional behavioral seams.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

from pc_instruction_emulator import run_bounded, INTEGRATION_ARENA_SIZE, PC_SHA256
from pc_loader_fixtures import PCFileBytesFixture, empty_manager, empty_fat, animation_rtti
from pc_stl_fixtures import install_char_traits
from inspect_pc_san_keys import DEFAULT, inspect, u32
import probe_pc_san_loader as original
from compare_pc_san_reader import compare
from compare_pc_san_file_roundtrip import compiled, emit, SOURCE, OUTPUT


class FileFixture(PCFileBytesFixture):
    guest_execution_profile = 'file'
    guest_arena_size = INTEGRATION_ARENA_SIZE


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('asset', choices=('bbush.san', 'bflower.san', 'barrel.san', 'bw.san'))
    parser.add_argument('--source-written', action='store_true')
    args = parser.parse_args(argv)
    start = time.monotonic()
    path = DEFAULT / args.asset
    if args.source_written: path, raw = emit(args.asset, lane='file-profile')
    else: raw = path.read_bytes()
    if not 36 <= len(raw) <= 65536: raise ValueError('Explicit small SAN file required')
    expected = json.loads(compiled('--inspect-file', path))
    expected.pop('usedPools')
    summary, tracks = inspect(path)
    f = FileFixture(raw); p = f.p
    report = {'kind': 'native-file-profile-comparison', 'asset': args.asset,
              'sourceWritten': args.source_written, 'inputSha256': hashlib.sha256(raw).hexdigest().upper(),
              'inputBytes': len(raw), 'pcExeSha256': PC_SHA256, 'executionProfile': p.execution_profile,
              'arenaLimitBytes': p.arena_size, 'processTimeoutSeconds': 30,
              'sourceExecutableSha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest().upper(),
              'status': 'started', 'phase': 'setup'}
    stages = (0x422260, 0x466b90, 0x465cd0, 0x4aa430, 0x4aab80, 0x422940,
              0x4143f0, 0x414420, 0x467550, 0x43ecc0, 0x466760, 0x466870)
    output = OUTPUT / (Path(args.asset).stem + ('-source' if args.source_written else '-original') + '-file-profile.json')
    OUTPUT.mkdir(parents=True, exist_ok=True)
    before = original.checks
    try:
        animation_rtti(f); install_char_traits(p)
        del p.seams[0x454370]; del p.seams[0x453b10]
        names = f.call(0x454640)
        fat = empty_fat(f); manager, _ = empty_manager(f)
        p.put_uint(manager + 0x28, fat); p.put_uint(0x75dde8, manager)
        serializer = f.call(0x43dab0)
        f.call(0x422d90, this=manager, args=(0x56ee563a, serializer, 0xff, 1))
        at = time.monotonic()
        report['phase'] = 'whole-load'
        animation = f.call(0x422b50, this=manager, args=(f.stream,))
        report.update(wholeLoadSeconds=time.monotonic() - at,
                      wholeLoadInstructions=sum(p.visits.values()), executionLimits=p.last_execution_limits,
                      visitedStages={f'{entry:08X}': p.visits[entry] for entry in stages})
        report['phase'] = 'capture-comparison'
        original.check(animation in f.allocations and p.uint(animation) == 0x6de6cc, 'original Animation factory')
        for entry in stages: original.check(entry in p.visits, f'original whole-file stage {entry:08X}')
        original.check(f.position == len(raw) and not f.errors, 'complete file consumed without diagnostics')
        original.check(p.uint(f.stream + 0x14) == u32(raw, 20), 'original data origin')
        original.check(all(op[2] == op[3] for op in f.io if op[0] == 'read'), 'no short reads')
        original.check(p.uint(fat + 0x28) == p.uint(fat + 0x34) == p.uint(fat + 0x50) == 0,
                       'original clears FAT maps and insertion list')
        observed = original.capture(f, animation, summary, tracks, raw)
        observed.pop('usedPools')
        # Native floats retain their type; JSON renders integral floats as
        # integers. Existing comparator selects its documented float tolerance
        # from the first (native) operand, not the JSON representation.
        count = compare(observed, expected)
        report['phase'] = 'cleanup'
        f.call(p.uint(p.uint(animation)), this=animation, args=(1,))
        f.call(0x4228a0, this=manager)
        for address in (0x75db78, 0x75526c):
            obj = p.uint(address)
            if obj: f.call(p.uint(p.uint(obj)), this=obj, args=(1,))
        f.call(0x4545d0, this=names, args=(1,))
        original.check(set(f.allocations) == set(f.freed), 'all original allocations released')
        report.update(status='passed', exactValues=count, nativeAssertions=original.checks - before,
                      releasedAllocations=len(f.freed), arenaReservedBytes=p.allocated,
                      tracks=len(tracks), capacity=observed['capacity'])
    except (AssertionError, ValueError) as error:
        report.update(status='stopped' if p.reg('EIP') != 0x32000000 else 'failed', error=str(error), executedInstructions=sum(p.visits.values()),
                      executionLimits=p.last_execution_limits, stoppedIp=f'{p.reg("EIP"):08X}',
                      fileCursor=f.position, arenaReservedBytes=p.allocated,
                      failureVisitedStages={f'{entry:08X}': p.visits[entry] for entry in stages})
        raise
    finally:
        report['elapsedSeconds'] = time.monotonic() - start
        output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('CAPTURE', json.dumps(report, sort_keys=True), flush=True)
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']: raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
