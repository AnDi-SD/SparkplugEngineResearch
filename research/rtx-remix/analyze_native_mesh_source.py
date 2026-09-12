"""Summarize a closed native mesh-source run; bounded logs are not whole-game coverage."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
from analyze_native_draw import records


def exited(pid):
    if os.name != 'nt':
        raise ValueError('Verify the recorded game process on Windows')
    import ctypes as c
    from ctypes import wintypes as w
    k = c.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    k.OpenProcess.restype = w.HANDLE
    k.GetExitCodeProcess.argtypes = [w.HANDLE, c.POINTER(w.DWORD)]
    k.CloseHandle.argtypes = [w.HANDLE]
    handle = k.OpenProcess(0x1000, False, pid)
    if not handle:
        if c.get_last_error() != 87:
            raise c.WinError(c.get_last_error())
        return
    try:
        code = w.DWORD()
        if not k.GetExitCodeProcess(handle, c.byref(code)):
            raise c.WinError(c.get_last_error())
        if code.value == 259:
            raise ValueError('Recorded game PID is still running')
    finally:
        k.CloseHandle(handle)


def analyze(run):
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    exited(launch['pid'])
    path = run / 'native-mesh-source.jsonl'
    before = path.stat()
    kinds, errors, origins, native_frames = Counter(), Counter(), Counter(), Counter()
    switches, comparisons, meshes = [], {}, set()
    total_used = total_calls = 0
    max_buffers = max_bytes = 0
    first = last = None
    init = None
    for row in records(path):
        kind = row['event']; kinds[kind] += 1
        if kind == 'init':
            if init is not None:
                raise ValueError('Duplicate native source initialization')
            init = row
        elif kind == 'frame':
            frame = row['frame']
            if last is not None and frame <= last:
                raise ValueError('Non-monotonic frame records')
            if first is None:
                first = frame
            last = frame
            errors.update({key: row.get(key, 0) for key in ('failures', 'limited', 'uploadMismatches')})
            max_buffers = max(max_buffers, row['buffers']); max_bytes = max(max_bytes, row['bytes'])
            if row['used']:
                native_frames[row['used']] += 1
            total_used += row['used']; total_calls += row['meshCalls']
        elif kind == 'compare_upload':
            generation = row['generation']
            comparisons[generation] = comparisons.get(generation, True) and row['equal']
        elif kind == 'submit':
            origins[row['geometrySource']] += 1; meshes.add(row['mesh'])
        elif kind == 'source_switch':
            switches.append(row)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('Native log changed during analysis')
    if not init or not init['enabled']:
        raise ValueError('Native source was not enabled')
    return dict(schema=1, run=str(run), proxySha256=launch['proxySha256'], pidExited=True,
                sourceLogSha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=before.st_size,
                logLimitReached=before.st_size >= init.get('maxLogBytes', 16 * 1024 * 1024),
                frameRange=[first, last], events=dict(kinds), observedErrors=dict(errors),
                comparedGenerations=len(comparisons), failedComparisons=sum(not v for v in comparisons.values()),
                nativeInstancesInRecordedFrames=total_used, meshCallsInRecordedFrames=total_calls,
                nativeFrameHistogram=dict(sorted(native_frames.items())), sampledInstanceSources=dict(origins),
                distinctMeshAddressesInSampledInstances=len(meshes), maxBuffers=max_buffers, maxBytes=max_bytes,
                switches=switches,
                scope='Recorded frames only. Mesh calls include other passes/UI; no whole-scene percentage or object lifetime identity inferred. Layout/material still use D3D.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.run)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    print(json.dumps(result))
