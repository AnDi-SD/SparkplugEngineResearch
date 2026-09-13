"""Validate a closed server audit; early means before first remaining D3D draw."""
import argparse
from collections import Counter
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path

MAX_BYTES = 16 * 1024 * 1024
SUM_FIELDS = {
    'apiCalls': 'totalCalls', 'rendererApiSuccess': 'totalSuccess',
    'rendererApiErrors': 'totalErrors', 'earlyCalls': 'totalEarlyCalls',
    'earlySuccess': 'totalEarlySuccess', 'earlyErrors': 'totalEarlyErrors',
    'invalidMeshHandles': 'totalInvalidMeshHandles',
}
DRAW_FIELDS = ('ordinaryDrawCommands', 'zeroDrawCommands', 'foreignDrawCommands', 'foreignPresents')
PRESENTS = {'device_present', 'implicit_swapchain_present'}
INCOMPLETE = {'registration_boundary', 'device_replaced', 'reset_begin',
              'implicit_swapchain_replaced', 'implicit_swapchain_destroy',
              'device_destroy', 'queue_exit_normal', 'queue_exit_unexpected'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def uint(row, key, bits=64):
    value = row.get(key)
    require(type(value) is int and 0 <= value < 1 << bits, f'Invalid {key}')
    return value


def signed(row, key):
    value = row.get(key)
    require(type(value) is int and -(1 << 31) <= value < 1 << 31, f'Invalid {key}')
    return value


def exited(pid):
    require(type(pid) is int and 0 < pid < 1 << 32, 'Invalid recorded PID')
    require(os.name == 'nt', 'Recorded process closure must be verified on Windows')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.WaitForSingleObject.restype = wintypes.DWORD
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x100000, False, pid)  # SYNCHRONIZE; no mutation.
    if not handle:
        error = ctypes.get_last_error()
        if error == 87:  # PID no longer exists.
            return
        raise ctypes.WinError(error)
    try:
        state = kernel.WaitForSingleObject(handle, 0)
        if state == 0xFFFFFFFF:
            raise ctypes.WinError(ctypes.get_last_error())
        require(state == 0, f'Recorded PID {pid} is still running (including PID reuse)')
    finally:
        kernel.CloseHandle(handle)


def identity(stat):
    return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns


def read_closed_file(path, limit):
    before = path.stat()
    require(before.st_size <= limit, f'Unexpected size: {path}')
    with path.open('rb') as stream:
        require(identity(os.fstat(stream.fileno())) == identity(before), 'File replaced before read')
        data = stream.read(limit + 1)
        require(identity(os.fstat(stream.fileno())) == identity(before), 'File changed during read')
    require(len(data) == before.st_size and identity(path.stat()) == identity(before), 'File changed during read')
    return data, identity(before)


def validate(data):
    """Pure bounded schema/accounting check; process/file gates live in analyze()."""
    require(0 < len(data) <= MAX_BYTES, 'Unexpected log size')
    require(data.endswith(b'\n'), 'Partial final JSONL row')
    rows = []
    for line in data.splitlines():
        require(0 < len(line) <= 4096, 'Empty or oversized audit row')
        row = json.loads(line)
        require(type(row) is dict, 'Audit row is not an object')
        rows.append(row)
    init = rows[0]
    require(init.get('event') == 'init' and init.get('schema') == 1 and
            init.get('scope') == 'registered_renderer_api_device' and
            init.get('firstDrawIncludesZeroAndFailure') is True and
            init.get('maxBytes') == MAX_BYTES, 'Unexpected audit initialization')
    require(uint(init, 'pid', 32) > 0, 'Invalid server PID')
    totals, events, boundaries, histogram = Counter(), Counter(init=1), Counter(), Counter()
    categories = {name: Counter() for name in ('qualifiedComplete', 'unqualifiedComplete', 'incomplete')}
    sequence = epoch = interval = present_index = 0
    previous_end = 0
    registered = False
    device = 0
    terminal = None
    fault_rows = []
    for row in rows[1:]:
        require(terminal is None, 'Events after terminal marker')
        event = row.get('event')
        require(event in ('register', 'interval', 'cap'), 'Unexpected audit event')
        events[event] += 1
        if event == 'cap':
            require(row.get('truncated') is True and row.get('furtherAuditDisabled') is True, 'Invalid cap')
            terminal = 'capped'
            continue
        current_epoch = uint(row, 'deviceEpoch')
        require(current_epoch >= epoch, 'Decreasing device epoch')
        if event == 'register':
            current_sequence = uint(row, 'commandSequence')
            require(current_sequence >= sequence and current_epoch > epoch, 'Invalid registration order')
            uint(row, 'uid', 32)
            device = uint(row, 'device', 32)
            registered = bool(device and signed(row, 'result') == 0)
            sequence, epoch = current_sequence, current_epoch
            continue
        end = uint(row, 'endSequence')
        require(end >= sequence and uint(row, 'interval') == interval and
                uint(row, 'presentIndex') == present_index, 'Invalid interval/command/Present order')
        require(type(row.get('complete')) is bool and type(row.get('qualifiedOrder')) is bool,
                'Invalid interval flags')
        boundary = row.get('boundary')
        require(boundary in PRESENTS | INCOMPLETE, 'Unknown interval boundary')
        complete, qualified = row['complete'], row['qualifiedOrder']
        require(complete == (boundary in PRESENTS), 'Boundary/complete mismatch')
        result = signed(row, 'presentResult')
        current_device = uint(row, 'device', 32)
        uint(row, 'implicitSwapchain', 32)
        require(current_device == device, 'Unexpected registered-device identity')
        for key in (*SUM_FIELDS, *SUM_FIELDS.values(), *DRAW_FIELDS):
            uint(row, key)
        for key in ('endUid', 'firstDrawUid', 'firstDrawKind', 'firstDrawPrimitiveCount', 'firstApiUid', 'lastApiUid'):
            uint(row, key, 32)
        require(row['rendererApiSuccess'] + row['rendererApiErrors'] == row['apiCalls'] and
                row['earlySuccess'] + row['earlyErrors'] == row['earlyCalls'], 'API partition mismatch')
        require(row['earlyCalls'] <= row['apiCalls'] and row['earlySuccess'] <= row['rendererApiSuccess'] and
                row['earlyErrors'] <= row['rendererApiErrors'] and row['invalidMeshHandles'] <= row['apiCalls'],
                'API subset mismatch')
        require(row['zeroDrawCommands'] <= row['ordinaryDrawCommands'], 'Zero draw subset mismatch')
        first_error = signed(row, 'firstRendererError')
        require(bool(first_error) == bool(row['rendererApiErrors']), 'Missing/unexpected first API error')
        first, last, draw = (uint(row, k) for k in ('firstApiSequence', 'lastApiSequence', 'firstDrawSequence'))
        if row['apiCalls']:
            require(0 < first <= last <= end and first >= sequence, 'API FIFO outside interval')
        else:
            require(first == last == row['firstApiUid'] == row['lastApiUid'] == 0, 'API metadata without calls')
        if row['ordinaryDrawCommands']:
            require(0 < draw <= end and draw >= sequence and 1 <= row['firstDrawKind'] <= 4,
                    'Draw FIFO outside interval')
            if row['apiCalls']:
                require(not row['earlyCalls'] or first <= draw, 'Early API starts after first draw')
                require(row['earlyCalls'] or first >= draw, 'Late API starts before first draw')
                require(row['earlyCalls'] != row['apiCalls'] or last <= draw, 'All-early API ends after first draw')
                require(row['earlyCalls'] == row['apiCalls'] or last >= draw, 'Late API ends before first draw')
        else:
            require(draw == row['firstDrawUid'] == row['firstDrawKind'] == row['firstDrawPrimitiveCount'] == 0,
                    'Draw metadata without draws')
            require(row['earlyCalls'] == row['apiCalls'], 'No ordinary draw but API marked late')
        if qualified:
            require(complete and registered and result >= 0 and not row['foreignDrawCommands'] and
                    not row['foreignPresents'], 'Unjustified qualified order')
        for key, cumulative in SUM_FIELDS.items():
            totals[key] += row[key]
            require(totals[key] == row[cumulative], f'Cumulative conservation failed: {key}')
        totals.update({key: row[key] for key in DRAW_FIELDS})
        category = 'qualifiedComplete' if qualified else 'unqualifiedComplete' if complete else 'incomplete'
        categories[category].update(intervals=1, **{key: row[key] for key in (*SUM_FIELDS, *DRAW_FIELDS)})
        boundaries[boundary] += 1
        histogram[(row['apiCalls'], row['earlyCalls'], row['rendererApiErrors'], row['invalidMeshHandles'],
                   row['ordinaryDrawCommands'], qualified)] += 1
        if (row['rendererApiErrors'] or row['invalidMeshHandles']) and len(fault_rows) < 100:
            fault_rows.append({key: row[key] for key in ('interval', 'boundary', 'apiCalls',
                              'rendererApiErrors', 'invalidMeshHandles', 'firstRendererError')})
        interval += 1
        present_index += int(complete)
        previous_end = sequence = end
        epoch = current_epoch
        if boundary == 'device_destroy':
            device, registered = 0, False
        if boundary.startswith('queue_exit_'):
            terminal = boundary
    return dict(schema=1, status='VALID_RECORDED_PREFIX', serverPid=init['pid'], events=dict(events),
                termination=terminal or 'missing_terminal_record', capReached=terminal == 'capped',
                auditQueueExitRecorded=bool(terminal and terminal.startswith('queue_exit_')),
                conservation='all emitted interval rows, including incomplete/unqualified; excludes any unlogged tail',
                totals=dict(totals), categories={k: dict(v) for k, v in categories.items()},
                boundaries=dict(boundaries), lastSequence=sequence, lastDeviceEpoch=epoch,
                intervals=interval, presentBoundaries=present_index,
                histogram=[dict(apiCalls=k[0], earlyCalls=k[1], rendererApiErrors=k[2], invalidMeshHandles=k[3],
                                ordinaryDrawCommands=k[4], qualifiedOrder=k[5], intervals=v)
                           for k, v in sorted(histogram.items())], faultIntervalsFirst100=fault_rows,
                earlyMeaning='before first remaining ordinary server D3D draw; no client source/lane attribution',
                rendererSuccessMeaning='renderer API return after EmitCs acceptance, not GPU completion')


def analyze(launch_path, log_path):
    launch_data, launch_identity = read_closed_file(launch_path, 1024 * 1024)
    launch = json.loads(launch_data.decode('utf-8-sig'))
    require('bridgeInstanceAudit' not in launch or launch['bridgeInstanceAudit'] is True, 'Audit launch disabled')
    exited(launch['pid'])
    data, log_identity = read_closed_file(log_path, MAX_BYTES)
    init = json.loads(data.splitlines()[0]) if data else {}
    exited(init.get('pid'))
    summary = validate(data)
    # Recheck both recorded PIDs and file identities after parsing, including PID reuse.
    exited(launch['pid'])
    exited(summary['serverPid'])
    require(identity(log_path.stat()) == log_identity and identity(launch_path.stat()) == launch_identity,
            'Evidence changed during analysis')
    summary.update(launchPid=launch['pid'], recordedPidsExited=True, immutableRead=True,
                   launch=str(launch_path), log=str(log_path), bytes=len(data),
                   launchSha256=hashlib.sha256(launch_data).hexdigest().upper(),
                   logSha256=hashlib.sha256(data).hexdigest().upper())
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--launch', type=Path, help='Explicit metadata path, e.g. closed CPU process.json')
    parser.add_argument('--log', type=Path, help='Explicit JSONL path, e.g. closed CPU sequence.jsonl')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = analyze(args.launch or args.run / 'launch.json', args.log or args.run / 'renderer-instance-audit.jsonl')
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        with args.output.open('x', encoding='utf-8') as stream:
            stream.write(text)
    else:
        print(text, end='')
