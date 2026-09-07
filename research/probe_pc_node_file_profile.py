#!/usr/bin/env python3
"""Fresh whole422B50 Node corpus comparisons using the explicit file profile."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from pc_instruction_emulator import ROOT, PC_SHA256, run_bounded
from pc_loader_fixtures import empty_manager, empty_fat
from probe_pc_san_file_profile import FileFixture, OUTPUT
from probe_pc_node_relationships import node_rtti, CLASS
from probe_pc_node_corpus import ASSETS
from probe_pc_san_reader import cstring

checks = 0


def check(condition, label):
    global checks
    checks += 1
    if not condition: raise AssertionError(label)


def capture(f, root):
    p = f.p; rows = []; nodes = []
    def visit(node):
        check(len(rows) < 32 and node not in nodes, 'bounded acyclic corpus graph')
        index = len(rows); nodes.append(node)
        name = p.uint(node + 0x10)
        label = cstring(p, name + 9).decode('latin1') if name else None
        state = b''.join(bytes(p.mu.mem_read(node + offset, count * 4)) for offset, count in
                         ((0x20, 3), (0x30, 3), (0x40, 9), (0x74, 3), (0x80, 3), (0x8c, 9)))
        row = [label, p.uint(node + 0xb0), state.hex(), []]; rows.append(row)
        head = p.uint(node + 0x18); at = p.uint(head)
        while at != head:
            child = p.uint(at + 8)
            check(p.uint(child + 0x2c) == node, 'reciprocal original parent')
            row[3].append(visit(child)); at = p.uint(at)
        return index
    visit(root)
    return rows, nodes


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('asset', choices=tuple(ASSETS))
    args = parser.parse_args(argv)
    start = time.monotonic()
    path = ROOT / 'local-data/pc-pristine/Media/Menus' / args.asset
    raw = path.read_bytes()
    check(len(raw) <= 512 and hashlib.sha256(raw).hexdigest().upper() == ASSETS[args.asset], 'unchanged tiny corpus hash')
    binary = ROOT / '.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugNodeSerializationTests.exe'
    source = subprocess.run([str(binary), '--asset', str(path)], capture_output=True, timeout=10, check=True)
    check(len(source.stdout) <= 65536, 'bounded portable output')
    expected = json.loads(source.stdout)
    f = FileFixture(raw); p = f.p
    report = {'kind': 'native-node-file-profile-comparison', 'asset': args.asset,
              'pcExeSha256': PC_SHA256, 'inputSha256': ASSETS[args.asset], 'inputBytes': len(raw),
              'executionProfile': p.execution_profile, 'arenaLimitBytes': p.arena_size,
              'processTimeoutSeconds': 30, 'sourceExecutableSha256': hashlib.sha256(binary.read_bytes()).hexdigest().upper(),
              'phase': 'setup', 'status': 'started'}
    stages = (0x422260, 0x466b90, 0x465cd0, 0x4aab80, 0x422940, 0x467550,
              0x421e20, 0x463a70, 0x4678b0, 0x421a60, 0x466760)
    try:
        node_rtti(f); f.call(0x6d38e0)
        fat = empty_fat(f); manager, _ = empty_manager(f)
        p.put_uint(manager + 0x28, fat); p.put_uint(0x75dde8, manager)
        serializer = f.call(0x4638f0)
        f.call(0x422d90, this=manager, args=(CLASS, serializer, 0xff, 3))
        scene = f.call(0x45adf0)
        check(p.uint(0x75db90) == scene and p.uint(scene + 0x1c) == 0, 'actual empty SceneManager startup')
        report['phase'] = 'whole-load'; at = time.monotonic()
        root = f.call(0x422b50, this=manager, args=(f.stream,))
        report.update(wholeLoadSeconds=time.monotonic() - at, wholeLoadInstructions=sum(p.visits.values()),
                      executionLimits=p.last_execution_limits,
                      visitedStages={f'{entry:08X}': p.visits[entry] for entry in stages})
        report['phase'] = 'capture-comparison'
        check(root in f.allocations and p.uint(root) == 0x6dc4f4, 'actual whole loader Node root')
        for entry in stages: check(entry in p.visits, f'actual whole-file stage {entry:08X}')
        check(not f.errors and f.position == len(raw), 'entire graph consumed without diagnostics')
        check(p.uint(fat + 0x28) == p.uint(fat + 0x34) == p.uint(fat + 0x50) == 0, 'FAT clear preserves graph')
        rows, nodes = capture(f, root)
        check(len(rows) == (2 if args.asset == 'object.smo' else 6), 'complete corpus node count')
        check(rows == expected, 'exact names, flags, local/world float bits and tree edges')
        report['phase'] = 'cleanup'
        f.call(0x422220, this=root, args=(1,))
        check(all(node in f.freed for node in nodes), 'root destroys complete native graph')
        f.call(0x4228a0, this=manager)
        for address in (0x75db90, 0x75db78, 0x75526c, 0x755264):
            obj = p.uint(address)
            if obj: f.call(p.uint(p.uint(obj)), this=obj, args=(1,))
        check(set(f.allocations) == set(f.freed), 'all native allocations freed')
        report.update(status='passed', exactNodeRecords=len(rows), nativeAssertions=checks,
                      releasedAllocations=len(f.freed), arenaReservedBytes=p.allocated)
    except (AssertionError, ValueError) as error:
        report.update(status='stopped' if p.reg('EIP') != 0x32000000 else 'failed', error=str(error),
                      executedInstructions=sum(p.visits.values()), executionLimits=p.last_execution_limits,
                      stoppedIp=f'{p.reg("EIP"):08X}', fileCursor=f.position, arenaReservedBytes=p.allocated,
                      failureVisitedStages={f'{entry:08X}': p.visits[entry] for entry in stages})
        raise
    finally:
        report['elapsedSeconds'] = time.monotonic() - start
        OUTPUT.mkdir(parents=True, exist_ok=True)
        (OUTPUT / (Path(args.asset).stem + '-node-file-profile.json')).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('CAPTURE', json.dumps(report, sort_keys=True), flush=True)
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']: raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
