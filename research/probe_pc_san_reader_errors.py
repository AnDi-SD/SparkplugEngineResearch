#!/usr/bin/env python3
"""Observe native SAN termination/failure quirks in bounded guest memory only."""
from pathlib import Path
import struct
import sys
import probe_pc_animation_lifecycle as lifetime
from probe_pc_animation_lifecycle import check
from probe_pc_san_reader import ReaderFixture
from pc_instruction_emulator import run_bounded


def run_case(label, data, expected_result, expected_position, expected_errors, expected_time=0):
    fixture = ReaderFixture(data)
    animation = fixture.call(0x41a090)
    serializer = fixture.call(0x43dab0)
    result = fixture.call(0x43ecc0, this=serializer + 0x10,
                          args=(fixture.stream, animation)) & 255
    print(label, 'result', result, 'position', fixture.position,
          'errors', fixture.errors, flush=True)
    check(result == expected_result, label + ' return')
    check(fixture.position == expected_position, label + ' consumed bytes')
    check(len(fixture.errors) == expected_errors, label + ' diagnostics')
    check(fixture.p.floats(animation + 0x14, 1)[0] == expected_time, label + ' retained object time')
    fixture.call(0x430130, this=animation)
    p = fixture.p
    fixture.call(p.uint(p.uint(serializer)), this=serializer, args=(1,))


def main():
    run_case('empty stream', b'', 1, 0, 1)
    run_case('ordinary terminator', b'\x00', 1, 1, 0)
    run_case('terminator with nonzero inline ID', b'\x05', 1, 1, 0)
    run_case('trailing bytes after terminator', b'\x00garbage', 1, 1, 0)
    run_case('missing terminator after total time', b'\x60' + struct.pack('<f', 2), 1, 5, 1, 2)
    run_case('truncated extended ID', b'\x7f', 1, 1, 1)
    run_case('truncated explicit field length', b'\xa0', 1, 1, 1)
    run_case('truncated total-time payload', b'\x60\x00', 0, 1, 1)
    run_case('payload failure retains earlier object mutation',
             b'\x60' + struct.pack('<f', 2) + b'\x60\x00', 0, 6, 1, 2)
    fixture = ReaderFixture(b'\x00')
    serializer = fixture.call(0x43dab0)
    for time in (2, 3):
        fixture.data = b'\x60' + struct.pack('<f', time) + b'\x00'
        fixture.position = 0
        animation = fixture.call(0x41a090)
        fixture.p.mu.mem_write(serializer + 0x14, b'\xcc' * 0x38)
        result = fixture.call(0x43ecc0, this=serializer + 0x10,
                              args=(fixture.stream, animation)) & 255
        check(result == 1, 'reused serializer succeeds')
        check(bytes(fixture.p.mu.mem_read(serializer + 0x14, 0x38)) == bytes(0x38),
              'all fourteen scratch counters reset per read')
        check(fixture.p.floats(animation + 0x14, 1)[0] == time, 'fresh target on reused reader')
        fixture.call(0x430130, this=animation)
    fixture.call(fixture.p.uint(fixture.p.uint(serializer)), this=serializer, args=(1,))
    print(f'PASS {lifetime.checks}/{lifetime.checks}: native reader termination/failure observations')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
