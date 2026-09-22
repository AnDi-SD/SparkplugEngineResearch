"""Prepare a fresh process-only fixture around the pinned upstream method."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

PACKAGE = Path(__file__).resolve().parent
ROOT = PACKAGE.parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('name')
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--kind', choices=('Wait', 'RemoteHandle'), default='Wait')
    args = parser.parse_args()
    if not args.name or any(not (c.isascii() and (c.isalnum() or c in '-_')) for c in args.name):
        raise ValueError('Use a simple, fresh experiment name')
    manifest = json.loads((PACKAGE / 'shutdown-manifest.json').read_text())
    original = args.source.resolve() / manifest['sourcePath']
    raw = original.read_bytes()
    text = raw.decode('utf-8').replace('\r\n', '\n')
    assert hashlib.sha256(text.encode()).hexdigest() == manifest['beforeLfSha256']
    if args.kind == 'RemoteHandle':
        text = text.replace("WaitForSingleObject(hProcess, 3'000)", "WaitForSingleObject(hProcess, 10'000)").replace('Give the child process 3 seconds', 'Give the child process 10 seconds')
        manifest = json.loads((PACKAGE / 'remote-handle-manifest.json').read_text())
        assert hashlib.sha256(text.encode()).hexdigest() == manifest['beforeLfSha256']
    start = text.index('  void Process::releaseChildProcess() {')
    end = text.index('  bool Process::RegisterExitCallback(', start)
    method = text[start:end]
    candidate = method.replace("WaitForSingleObject(hProcess, 3'000)", "WaitForSingleObject(hProcess, 10'000)").replace('3 seconds', '10 seconds')
    if args.kind == 'RemoteHandle':
        remove = '''    // Also close the duplicate client process handle that we created for the server
    if (INVALID_HANDLE_VALUE != hDuplicate) {
      CloseHandle(hDuplicate);
    }
'''
        replacement = '''    // Own bridge fix: hDuplicate belongs to the server's handle table.
    // The server process releases that handle when it exits.
'''
        assert method.count(remove) == 1
        candidate = method.replace(remove, replacement)
    assert hashlib.sha256(method.encode()).hexdigest() == manifest['beforeMethodSha256']
    assert hashlib.sha256(candidate.encode()).hexdigest() == manifest['afterMethodSha256']
    notice = text[:text.index('#include')]
    harness = '''// Own fields; callback registration is outside these method tests.
class Process {
public:
  HANDLE hProcess=INVALID_HANDLE_VALUE;
  HANDLE hDuplicate=INVALID_HANDLE_VALUE;
  void UnregisterExitCallback() {}
  void releaseChildProcess();
};
'''
    work = ROOT / 'local-data/rtx-remix/bridge-shutdown-tests' / args.name
    work.mkdir(parents=True, exist_ok=False)
    generated = notice+'#pragma once\nnamespace baseline {\n'+harness+method+'}\nnamespace candidate {\n'+harness+candidate+'}\n'
    (work / 'shutdown_methods.h').write_bytes(generated.encode())
    (work / 'upstream_util_process.cpp').write_bytes(raw)
    test_source = 'test_child_remote_handle.cpp' if args.kind == 'RemoteHandle' else 'test_child_shutdown.cpp'
    shutil.copy2(PACKAGE / test_source, work / 'test_shutdown.cpp')
    for name in ('Prepare-ChildShutdownTest.py', 'Analyze-ChildShutdown.py', 'Test-ChildShutdown.ps1', 'shutdown-manifest.json'):
        shutil.copy2(PACKAGE / name, work / name)
    if args.kind == 'RemoteHandle':
        shutil.copy2(PACKAGE / 'remote-handle-manifest.json', work / 'remote-handle-manifest.json')
    source = dict(pin=manifest['baseRevision'], kind=args.kind, upstreamRawSha256=sha(original),
                  scope=('Exact extracted method body. Callback registration and remote duplicated handle excluded.'
                         if args.kind == 'Wait' else 'Exact extracted release method with real own process handle tables. DuplicateHandle arguments mirror the upstream sender; callback registration excluded.'),
                  files={p.name: sha(p) for p in work.iterdir() if p.is_file()})
    (work / 'source.json').write_bytes((json.dumps(source, indent=2)+'\n').encode())
    print(work)


if __name__ == '__main__':
    main()
