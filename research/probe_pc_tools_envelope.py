"""Whole original PC Node loader on a fresh tool-written envelope, existing file fixture."""
from pathlib import Path
import hashlib, json, subprocess, sys, time, traceback
from pc_instruction_emulator import ROOT, PC_SHA256, run_bounded
from pc_loader_fixtures import empty_fat, empty_manager
from probe_pc_san_file_profile import FileFixture
from probe_pc_node_relationships import node_rtti, CLASS
from probe_pc_node_file_profile import capture

FOLDER = ROOT / 'local-data/results/tools-core-cycle-20260911-1900/envelope'

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def main(argv):
    path, output = map(lambda s: Path(s).resolve(), argv)
    assert path.is_relative_to(FOLDER) and output.is_relative_to(FOLDER) and not output.exists()
    raw = path.read_bytes()
    assert 36 <= len(raw) <= 512
    binary = ROOT / 'artifacts/native/viewer/Release/ViewerNodeSerializationChecks.exe'
    result = dict(kind='tool-envelope-original-PC-reader', input=str(path.relative_to(ROOT)),
        inputSha256=sha(path), inputBytes=len(raw), pcExeSha256=PC_SHA256,
        sourceBinary=str(binary.relative_to(ROOT)), sourceBinarySha256=sha(binary),
        probeSha256=sha(Path(__file__)), status='started',
        scope='Original whole 422B50 and cleanup in existing file/allocator/name fixtures; not original Save or OS startup.')
    start = time.monotonic()
    try:
        source = subprocess.run([str(binary), '--asset', str(path)], capture_output=True, timeout=10, check=True)
        assert len(source.stdout) <= 65536
        expected = json.loads(source.stdout)
        f = FileFixture(raw); p = f.p
        result.update(profile=p.execution_profile, arenaBytes=p.arena_size)
        node_rtti(f); f.call(0x6d38e0)
        fat = empty_fat(f); manager, _ = empty_manager(f)
        p.put_uint(manager + 0x28, fat); p.put_uint(0x75dde8, manager)
        serializer = f.call(0x4638f0)
        f.call(0x422d90, this=manager, args=(CLASS, serializer, 0xff, 3))
        scene = f.call(0x45adf0)
        assert p.uint(0x75db90) == scene and p.uint(scene + 0x1c) == 0
        root = f.call(0x422b50, this=manager, args=(f.stream,))
        assert root in f.allocations and p.uint(root) == 0x6dc4f4
        assert not f.errors and f.position == len(raw)
        assert p.uint(fat + 0x28) == p.uint(fat + 0x34) == p.uint(fat + 0x50) == 0
        rows, nodes = capture(f, root)
        assert rows == expected
        result.update(nodeRecords=rows, wholeLoadInstructions=sum(p.visits.values()),
            limits=p.last_execution_limits, fileCursor=f.position)
        f.call(0x422220, this=root, args=(1,))
        assert all(node in f.freed for node in nodes)
        f.call(0x4228a0, this=manager)
        for address in (0x75db90, 0x75db78, 0x75526c, 0x755264):
            obj = p.uint(address)
            if obj: f.call(p.uint(p.uint(obj)), this=obj, args=(1,))
        assert set(f.allocations) == set(f.freed)
        result.update(status='passed', freedAllocations=len(f.freed))
    except Exception as error:
        result.update(status='blocked', error=str(error), traceback=traceback.format_exc())
    result['seconds'] = time.monotonic() - start
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('status','seconds','error') if k in result}))
    return int(result['status'] != 'passed')

if __name__ == '__main__':
    args=sys.argv[1:]
    raise SystemExit(main(args[1:]) if args[:1]==['--guest'] else run_bounded(Path(__file__),args))
