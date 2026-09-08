"""Original PC whole animation field reader: repeated PRS fields before a name."""
from pathlib import Path
import hashlib
import json
import math
import struct
import sys
from pc_instruction_emulator import ROOT, run_bounded
from probe_pc_san_reader import ReaderFixture, bits


def field(kind, data):
    return (bytes([0xe0 | kind]) if kind < 31 else bytes([0xff, kind])) + struct.pack('<I', len(data)) + data


def guest():
    rows = []
    unused = '--unused-coefficients' in sys.argv
    for count in ([1] if unused else [0, 1]):
        pools = [0, 0, 0, 1, 0, 0, 1] if unused else [0, 0, count*2, 0, 0, 0, count*2]
        data = b''.join(field(i+6, struct.pack('<I', n)) for i, n in enumerate(pools))
        data += field(64, struct.pack('<I', 1))
        for value in ([1] if unused else [1, 4]):
            payload = struct.pack('<II', 2 if unused else 1, count)
            if unused: payload += struct.pack('<16f', 0, 1, 2, 3, *([0]*6), *([float('nan')]*6))
            elif count: payload += struct.pack('<4f', 0, value, value+1, value+2)
            data += field(2, payload)
        data += field(1, b'\x07\x00Pelvis\x00') + b'\x00'
        f = ReaderFixture(data); p = f.p
        animation = f.call(0x41a090); serializer = f.call(0x43dab0)
        result = f.call(0x43ecc0, this=serializer+0x10, args=(f.stream, animation)) & 255
        assert result == 1 and f.position == len(data) and not f.errors
        assert p.uint(animation+0x20) == 1
        track = p.uint(animation+0x1c); output = p.allocate(40); flags = p.allocate(3); cache = p.allocate(36)
        p.put_floats(output, [0, 0, 0, 0, 0, 0, 1, 1, 1, 1])
        f.call(0x479290, this=track, args=(bits(0), cache, cache+12, cache+24,
            output, output+12, output+28, flags, flags+1, flags+2))
        position = list(p.floats(output, 3))
        row = {'keyCountPerField': count, 'readerResult': result, 'consumed': f.position,
            'fieldBytesSha256': hashlib.sha256(data).hexdigest(),
            'position': [v if math.isfinite(v) else str(v) for v in position],
            'positionBits': [f'{p.uint(output+i*4):08X}' for i in range(3)],
            'finite': all(math.isfinite(v) for v in position), 'validity': list(p.mu.mem_read(flags, 3))}
        rows.append(row)
        f.call(0x430130, this=animation)
        f.call(p.uint(p.uint(serializer)), this=serializer, args=(1,))
    output = ROOT/'local-data/results/viewer-core-audit-20260908'/('unused-coefficients.json' if unused else 'repeated-role.json')
    report = {'reader': '43ECC0', 'keyReader': '43DB90', 'sampler': '479290', 'rows': rows,
        'exeSha256': hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest(),
        'scope': ('Single packed cubic key with unused NaN coefficients' if unused else 'Two repeated linear position cases')
            + '; stream/name/allocator/CRT fixtures; original whole field reader',
        'limits': '64KiB guest; 100000 instructions/2s per call; 30s outer'}
    output.write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print(json.dumps(report, allow_nan=False))


if __name__ == '__main__':
    if '--guest' in sys.argv: guest()
    else: raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
