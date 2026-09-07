#!/usr/bin/env python3
"""Batched byte-candidate xrefs, CPU or optional Windows OpenCL GPU.

These are unaligned abs32/E8-rel32 byte matches, NOT a disassembled call graph.
No original instructions execute. OpenCL 1.2 ABI reference:
https://github.com/KhronosGroup/OpenCL-Headers/blob/main/CL/cl.h
"""
from __future__ import annotations
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import queue
import threading
import time

from inspect_serializer_manager import PC_SHA256, read_pe, sha256

ROOT = Path(__file__).resolve().parents[1]
MAX_IMAGE = 64 * 1024 * 1024
MAX_TARGETS = 8192
HITS_PER_TARGET = 32768
MAX_TOTAL_HITS = 500000

KERNEL = r'''
uint word(__global const uchar *b, uint i) {
    return (uint)b[i] | ((uint)b[i+1]<<8) | ((uint)b[i+2]<<16) | ((uint)b[i+3]<<24);
}
void record(uint value, uint location, __global const uint *targets, uint n,
            __global uint *counts, __global uint *hits, uint capacity) {
    uint lo=0, hi=n;
    while (lo<hi) { uint mid=lo+(hi-lo)/2; if (targets[mid]<value) lo=mid+1; else hi=mid; }
    if (lo<n && targets[lo]==value) {
        uint slot=atomic_inc(counts);
        if (slot<capacity) { hits[slot*2]=lo; hits[slot*2+1]=location; }
    }
}
__kernel void scan(__global const uchar *bytes, uint length, uint base,
                   __global const uint *targets, uint n, __global uint *counts,
                   __global uint *hits, uint capacity) {
    uint i=(uint)get_global_id(0);
    if (i+4<=length) record(word(bytes,i),base+i,targets,n,counts,hits,capacity);
    if (i+5<=length && bytes[i]==0xe8)
        record(word(bytes,i+1)+base+i+5,(base+i)|0x80000000U,targets,n,counts,hits,capacity);
}
'''


def validate(data, base, targets):
    values = list(targets)
    if not 0 < len(data) <= MAX_IMAGE or not 0 <= base < 0x80000000 or base + len(data) > 0x80000000:
        raise ValueError('Bounded low-address image required')
    if not 0 < len(values) <= MAX_TARGETS or any(type(x) is not int or not 0 <= x <= 0xffffffff for x in values):
        raise ValueError('One to 8192 unsigned 32-bit targets required')
    return sorted(set(values))


def cpu_scan(data, base, targets, *, method='auto'):
    values = validate(data, base, targets)
    result = {v: [] for v in values}
    total = 0
    def append(bucket, location):
        nonlocal total
        if len(bucket) == HITS_PER_TARGET: raise ValueError(f'Explicit per-target candidate limit exceeded: {value:08X}')
        if total == MAX_TOTAL_HITS: raise ValueError('Explicit aggregate candidate limit exceeded')
        total += 1
        bucket.append(location)
    if method not in ('auto', 'find', 'words'): raise ValueError('Unknown CPU scan method')
    if (method == 'words' or method == 'auto' and len(values) > 32) and sys.byteorder == 'little':
        for shift in range(min(4, len(data))):
            end = len(data) - (len(data) - shift) % 4
            for index, value in enumerate(memoryview(data)[shift:end].cast('I')):
                bucket = result.get(value)
                if bucket is not None: append(bucket, base + shift + 4 * index)
    else:
        for value in values:
            encoded = struct.pack('<I', value)
            at = 0
            while (at := data.find(encoded, at)) >= 0:
                append(result[value], base + at); at += 1
    at = 0
    while (at := data.find(b'\xe8', at)) >= 0:
        if at + 5 <= len(data):
            value = (base + at + 5 + struct.unpack_from('<I', data, at + 1)[0]) & 0xffffffff
            if value in result: append(result[value], (base + at) | 0x80000000)
        at += 1
    return {v: sorted(hits) for v, hits in result.items()}


class OpenCLScan:
    def __init__(self, data, base):
        validate(data, base, [base])
        self.base, self.length, self.owned = base, len(data), []
        if sys.platform != 'win32': raise OSError('This optional OpenCL binding targets Windows')
        self.cl = c.WinDLL('OpenCL.dll')
        p, u, z, q, i = c.c_void_p, c.c_uint, c.c_size_t, c.c_ulonglong, c.c_int
        signatures = {
            'clGetPlatformIDs': (i, [u, p, p]), 'clGetDeviceIDs': (i, [p, q, u, p, p]),
            'clGetDeviceInfo': (i, [p, u, z, p, p]), 'clCreateContext': (p, [p, u, p, p, p, p]),
            'clCreateCommandQueue': (p, [p, p, q, p]),
            'clCreateProgramWithSource': (p, [p, u, p, p, p]), 'clBuildProgram': (i, [p, u, p, p, p, p]),
            'clGetProgramBuildInfo': (i, [p, p, u, z, p, p]), 'clCreateKernel': (p, [p, p, p]),
            'clCreateBuffer': (p, [p, q, z, p, p]), 'clSetKernelArg': (i, [p, u, z, p]),
            'clEnqueueNDRangeKernel': (i, [p, p, u, p, p, p, u, p, p]),
            'clEnqueueReadBuffer': (i, [p, p, u, z, z, p, u, p, p]),
        }
        for name in ('Context', 'CommandQueue', 'Program', 'Kernel', 'MemObject'):
            signatures['clRelease' + name] = (i, [p])
        for name, (result, args) in signatures.items():
            function = getattr(self.cl, name); function.restype = result; function.argtypes = args
        try:
            count = u(); self.check(self.cl.clGetPlatformIDs(0, None, c.byref(count)))
            if not 0 < count.value <= 32: raise OSError('No bounded OpenCL platform list')
            platforms = (p * count.value)(); self.check(self.cl.clGetPlatformIDs(count, platforms, None))
            device = None
            for platform in platforms:
                n = u()
                if self.cl.clGetDeviceIDs(platform, 4, 0, None, c.byref(n)) or not n.value: continue
                devices = (p * min(n.value, 32))()
                self.check(self.cl.clGetDeviceIDs(platform, 4, len(devices), devices, None))
                device = p(devices[0]); break
            if device is None: raise OSError('No OpenCL GPU device')
            self.device = device
            name = c.create_string_buffer(1024)
            self.check(self.cl.clGetDeviceInfo(device, 0x102b, len(name), name, None))
            self.device_name = name.value.decode('utf-8', errors='replace')
            error = i()
            self.context = self.handle('Context', self.cl.clCreateContext(None, 1, c.byref(device), None, None, c.byref(error)), error)
            self.queue = self.handle('CommandQueue', self.cl.clCreateCommandQueue(self.context, device, 0, c.byref(error)), error)
            source = c.c_char_p(KERNEL.encode('ascii'))
            self.program = self.handle('Program', self.cl.clCreateProgramWithSource(self.context, 1, c.byref(source), None, c.byref(error)), error)
            code = self.cl.clBuildProgram(self.program, 1, c.byref(device), c.c_char_p(b'-cl-std=CL1.2'), None, None)
            if code:
                log = c.create_string_buffer(16384)
                self.cl.clGetProgramBuildInfo(self.program, device, 0x1183, len(log), log, None)
                raise RuntimeError(f'OpenCL build {code}: {log.value.decode(errors="replace")}')
            self.kernel = self.handle('Kernel', self.cl.clCreateKernel(self.program, c.c_char_p(b'scan'), c.byref(error)), error)
            self.input = self.buffer(len(data), c.c_char_p(data), read_only=True)
        except BaseException:
            self.close(); raise

    @staticmethod
    def check(code):
        if code: raise RuntimeError(f'OpenCL error {code}')

    def handle(self, kind, value, error):
        self.check(error.value)
        if not value: raise RuntimeError('OpenCL returned a null handle')
        result = c.c_void_p(value); self.owned.append((kind, result)); return result

    def buffer(self, size, data=None, *, read_only=False):
        error = c.c_int()
        flags = (4 if read_only else 1) | (32 if data is not None else 0)
        return self.handle('MemObject', self.cl.clCreateBuffer(self.context, flags, size, data, c.byref(error)), error)

    def close(self, keep=0):
        while len(self.owned) > keep:
            kind, value = self.owned.pop(); getattr(self.cl, 'clRelease' + kind)(value)

    def scan(self, targets):
        # Validate query limits without duplicating the resident image.
        values = validate(b'\0', self.base, targets)
        n = len(values); keep = len(self.owned)
        try:
            query = (c.c_uint * n)(*values); count = c.c_uint()
            target_buffer = self.buffer(c.sizeof(query), query, read_only=True)
            count_buffer = self.buffer(c.sizeof(count), c.byref(count))
            hit_buffer = self.buffer(MAX_TOTAL_HITS * 8)
            args = [self.input, c.c_uint(self.length), c.c_uint(self.base), target_buffer,
                    c.c_uint(n), count_buffer, hit_buffer, c.c_uint(MAX_TOTAL_HITS)]
            for index, value in enumerate(args):
                self.check(self.cl.clSetKernelArg(self.kernel, index, c.sizeof(value), c.byref(value)))
            work = c.c_size_t(self.length)
            self.check(self.cl.clEnqueueNDRangeKernel(self.queue, self.kernel, 1, None, c.byref(work), None, 0, None, None))
            self.check(self.cl.clEnqueueReadBuffer(self.queue, count_buffer, 1, 0, c.sizeof(count), c.byref(count), 0, None, None))
            if count.value > MAX_TOTAL_HITS: raise ValueError('Explicit aggregate candidate limit exceeded')
            hits = (c.c_uint * (count.value * 2))()
            if len(hits):
                self.check(self.cl.clEnqueueReadBuffer(self.queue, hit_buffer, 1, 0, c.sizeof(hits), hits, 0, None, None))
            result = {v: [] for v in values}
            for index in range(count.value):
                value = values[hits[2 * index]]; bucket = result[value]
                if len(bucket) == HITS_PER_TARGET: raise ValueError(f'Explicit per-target candidate limit exceeded: {value:08X}')
                bucket.append(hits[2 * index + 1])
            return {value: sorted(bucket) for value, bucket in result.items()}
        finally:
            self.close(keep)


def pristine_image():
    raw = (ROOT / 'local-data/pc-pristine/WinxClub.exe').read_bytes()
    if sha256(raw) != PC_SHA256: raise ValueError('Pristine PC image required')
    base, sections = read_pe(raw)
    size = max(section.address + section.size for section in sections)
    if not 0 < size <= MAX_IMAGE: raise ValueError('Bounded image required')
    image = bytearray(size)
    for section in sections:
        image[section.address:section.address + section.size] = raw[section.offset:section.offset + section.size]
    return bytes(image), base


def serve():
    """Local stdin/stdout worker: pinned byte snapshot only, no guest execution.

    Keeps the OpenCL context between queries. Exits after 15 minutes idle or
    a quit request, releasing GPU memory. Reports go to an ignored cache.
    """
    scanner_sha = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    data, base = pristine_image()
    start = time.monotonic(); gpu = OpenCLScan(data, base)
    inbox = queue.Queue(maxsize=1)
    def receive():
        while True:
            line = sys.stdin.readline(262145)
            inbox.put(line)
            if not line: break
    threading.Thread(target=receive, daemon=True).start()
    print(json.dumps({'status': 'ready', 'backend': 'gpu-resident', 'device': gpu.device_name,
                      'pcExeSha256': PC_SHA256, 'setupSeconds': time.monotonic() - start,
                      'idleTimeoutSeconds': 900}), flush=True)
    try:
        while True:
            try: line = inbox.get(timeout=900)
            except queue.Empty: break
            if not line: break
            try:
                if len(line) > 262144:
                    print(json.dumps({'status': 'failed', 'error': 'Bounded request exceeded; worker exiting'}), flush=True)
                    break
                request = json.loads(line)
                if not isinstance(request, dict): raise ValueError('JSON object request required')
                if request.get('quit'): break
                at = time.monotonic(); result = gpu.scan(request['targets'])
                elapsed = time.monotonic() - at
                identity = {'pcExeSha256': PC_SHA256,
                            'scannerSha256': scanner_sha,
                            'targets': sorted(result)}
                digest = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
                directory = ROOT / 'local-data/research-cache/xrefs'; directory.mkdir(parents=True, exist_ok=True)
                path = directory / (digest + '.json')
                report = {**identity, 'backend': 'gpu-resident', 'elapsedSeconds': elapsed,
                          'meaning': 'Byte candidates only; disassembly is required before semantic use.',
                          'candidateCount': sum(map(len, result.values())),
                          'hits': {f'{target:08X}': [{'kind': 'rel32' if hit & 0x80000000 else 'abs32',
                                                    'address': f'{hit & 0x7fffffff:08X}'} for hit in hits]
                                   for target, hits in result.items()}}
                path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                print(json.dumps({'status': 'passed', 'targets': len(result), 'candidateCount': report['candidateCount'],
                                  'elapsedSeconds': elapsed, 'reportPath': path.relative_to(ROOT).as_posix()}), flush=True)
            except (ValueError, KeyError, TypeError, RuntimeError) as error:
                print(json.dumps({'status': 'failed', 'error': str(error)}), flush=True)
    finally:
        gpu.close()
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('targets', nargs='*', type=lambda x: int(x, 16))
    parser.add_argument('--targets-file', type=Path, help='JSON array of uint32 targets (max 256 KiB)')
    parser.add_argument('--backend', choices=('cpu', 'gpu', 'compare'), default='cpu')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(argv)
    if args.targets_file:
        if args.targets: parser.error('Use either explicit targets or --targets-file')
        with args.targets_file.open('rb') as stream: encoded = stream.read(262145)
        if len(encoded) > 262144: raise ValueError('Bounded target file exceeded')
        args.targets = json.loads(encoded)
    start = time.monotonic(); data, base = pristine_image(); prepared = time.monotonic()
    result = None; timings = {'prepareSeconds': prepared - start}
    if args.backend in ('cpu', 'compare'):
        at = time.monotonic(); result = cpu_scan(data, base, args.targets); timings['cpuSeconds'] = time.monotonic() - at
    if args.backend in ('gpu', 'compare'):
        at = time.monotonic(); gpu = OpenCLScan(data, base); timings['gpuSetupSeconds'] = time.monotonic() - at
        try:
            at = time.monotonic(); other = gpu.scan(args.targets); timings['gpuScanSeconds'] = time.monotonic() - at
            if result is not None and result != other: raise AssertionError('Independent CPU/GPU xref mismatch')
            result = other; timings['device'] = gpu.device_name
        finally: gpu.close()
    report = {'kind': 'byte-xref-candidates', 'pcExeSha256': PC_SHA256, 'kernelSha256': hashlib.sha256(KERNEL.encode()).hexdigest(),
              'backend': args.backend, 'imageBytes': len(data), 'timings': timings,
              'candidateCount': sum(map(len, result.values())), 'targets': {
                  f'{value:08X}': [{'kind': 'rel32' if hit & 0x80000000 else 'abs32', 'address': f'{hit & 0x7fffffff:08X}'}
                                  for hit in hits] for value, hits in result.items()},
              'meaning': 'Byte candidates only; verify instruction boundaries and callers before treating as semantic xrefs.'}
    if args.output: args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'candidateCount': report['candidateCount'], 'targets': len(result), 'timings': timings}, sort_keys=True))
    if not args.output:
        print(json.dumps(report['targets'], sort_keys=True))
    return 0


if __name__ == '__main__':
    if sys.argv[1:] == ['--serve']: raise SystemExit(serve())
    if sys.argv[1:2] == ['--worker']: raise SystemExit(main(sys.argv[2:]))
    try:
        raise SystemExit(subprocess.run([sys.executable, str(Path(__file__)), '--worker', *sys.argv[1:]], cwd=ROOT, timeout=30).returncode)
    except subprocess.TimeoutExpired:
        raise SystemExit('Bounded xref worker exceeded 30 seconds')
