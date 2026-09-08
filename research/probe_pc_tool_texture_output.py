#!/usr/bin/env python3
"""Read an actual tool-written TextureData slice with the original PC serializer.

Original 42C640/42C3B0/4ABBA0 and missing-mip helpers execute unchanged. The
explicit COM boundary supplies bounded BGRA surfaces, including declared NPOT
sizes. This proves original-reader acceptance, not a real GPU/game scene test.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys
import time

from analyze_smo_texture_data import parse_sections
from pc_instruction_emulator import ROOT, run_bounded
from pc_loader_fixtures import empty_manager
from pc_texture_mip_fixtures import TextureMipChainFixture
from probe_pc_texture_missing_mips import MissingMipFixture


class ToolOutputFixture(MissingMipFixture):
    def __init__(self, payload, dimensions):
        if len(dimensions) != 2 or any(n < 1 or n > 32 for n in dimensions):
            raise ValueError('declared 1..32 dimensions only')
        self.dimensions = dimensions
        self.max_levels = 6
        self.max_surface_bytes = 8192
        TextureMipChainFixture.__init__(self, payload)
        self.install_external_inputs()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def texture_slice(raw, selected):
    if len(raw) < 36 or raw[:4] != b'FFPS':
        raise ValueError('FFPS header required')
    file_size, _, data_start, data_size, count = struct.unpack_from('<5I', raw, 12)
    if file_size != len(raw) or data_start + data_size != len(raw) or not 0 <= selected < count <= 100000:
        raise ValueError('bounded valid FFPS catalog required')
    offset = 32
    found = None
    for index in range(count):
        identity, name_size = struct.unpack_from('<IH', raw, offset)
        offset += 6
        name = raw[offset:offset + name_size]
        offset += name_size
        kind, logical, size = struct.unpack_from('<3I', raw, offset)
        offset += 12
        if offset > data_start - 4 or logical + size > data_size:
            raise ValueError('catalog entry outside its section')
        if index == selected:
            if kind != 0x78ea082b or size < 9:
                raise ValueError('selected catalog entry must be spTextureData')
            start = data_start + logical
            if raw[start:start + 8] != struct.pack('<II', kind, 0x4f4f4253):
                raise ValueError('catalog/SBOO signature mismatch')
            found = (raw[start + 8:start + size], dict(objectIndex=index,
                     objectId=identity, nameHex=name.hex(), offset=start, size=size))
    if offset != data_start - 4 or raw[offset:data_start] != bytes(4):
        raise ValueError('catalog terminator mismatch')
    return found


def expected_base(payload):
    outer = parse_sections(payload)
    if len(outer) != 1 or outer[0].type != 3:
        raise ValueError('supported embedded source field3 required')
    derived = parse_sections(outer[0].payload)
    specific = [field for field in derived if field.section == 1 and field.type == 1]
    if len(specific) != 1:
        raise ValueError('single Direct3D representation required')
    fields = parse_sections(specific[0].payload)
    if len(fields) != 1 or fields[0].type != 0:
        raise ValueError('single explicit base mip required')
    raw = fields[0].payload
    width, height, flags = struct.unpack_from('<3I', raw, 1)
    mip_width, stride, mip_height = struct.unpack_from('<3I', raw, 14)
    if raw[0] != 1 or raw[13] != 1 or flags != 0 or (mip_width, stride, mip_height) != (width, width * 4, height):
        raise ValueError('consistent BGRA descriptor required')
    pixels = raw[26:]
    if len(pixels) != width * height * 4:
        raise ValueError('pixel size mismatch')
    return (width, height), pixels


def main(path, selected, report_path):
    source = Path(path).resolve()
    target = Path(report_path).resolve()
    source.relative_to(ROOT)
    target.relative_to(ROOT / 'local-data/results')
    if source.stat().st_size > 8 * 1024 * 1024:
        raise ValueError('tool-output fixture limited to 8 MiB input')
    raw = source.read_bytes()
    payload, metadata = texture_slice(raw, int(selected))
    dimensions, expected = expected_base(payload)
    if len(payload) > 8192:
        raise ValueError('bounded serialized slice')
    f = ToolOutputFixture(payload, dimensions)
    p = f.p
    manager, _ = empty_manager(f)
    p.put_uint(manager + 0x10, 2)
    p.put_uint(manager + 0x14, 1)
    p.put_uint(0x75dde8, manager)
    obj = f.call(0x4ab520)
    serializer = f.call(0x42b660)
    report = dict(kind='original-pc-tool-texture-acceptance', schemaVersion=1,
                  inputPath=str(source.relative_to(ROOT)), inputSha256=sha(raw),
                  payloadSha256=sha(payload), pixelSha256=sha(expected), dimensions=dimensions,
                  catalog=metadata, executionProfile='file', arenaLimitBytes=p.arena_size,
                  maxSurfaceBytes=f.max_surface_bytes, boundary='declared COM storage, no GPU')
    started = time.monotonic()
    try:
        result = f.call(0x42c640, this=serializer + 0x10, args=(f.stream, obj)) & 255
        report.update(readerResult=result, cursor=f.position, nativeInstructions=sum(p.visits.values()),
                      nativeState=[p.uint(obj + off) for off in (0x18, 0x20, 0x28, 0x2c)],
                      missingMipHelperVisits=p.visits.get(0x61039a, 0))
        assert result == 1 and f.position == len(payload) and not f.errors, 'complete original serializer read'
        assert (p.uint(obj + 0x28), p.uint(obj + 0x2c)) == dimensions, 'native serialized dimensions'
        levels = []
        for rec in f.levels:
            pixels = b''.join(bytes(p.mu.mem_read(rec['pixels'] + row * rec['pitch'], rec['row_bytes']))
                              for row in range(rec['rows']))
            assert all(bytes(p.mu.mem_read(rec['pixels'] + row * rec['pitch'] + rec['row_bytes'], 4)) == b'\xa5' * 4
                       for row in range(rec['rows'])), 'destination padding preserved'
            if rec['index'] == 0:
                assert pixels == expected, 'native base mip matches exact tool-written BGRA'
            levels.append(dict(width=rec['width'], height=rec['height'], pixelSha256=sha(pixels)))
        report.update(levels=levels, events=f.events.copy())
        f.call(0x4abb50, this=obj, args=(1,))
        f.call(p.uint(p.uint(serializer)), this=serializer, args=(1,))
        f.call(0x4228a0, this=manager)
        for address in (0x75db78, 0x75526c, 0x755264):
            owned = p.uint(address)
            if owned:
                f.call(p.uint(p.uint(owned)), this=owned, args=(1,))
        assert set(f.allocations) == set(f.freed), 'all native allocations released'
        assert f.texture_refs == f.surface_refs == 0 and f.device_refs == 1, 'COM ownership balanced'
        assert all(rec['refs'] == 0 and not rec['locked'] for rec in f.levels), 'mip lifetimes balanced'
        report.update(status='passed', releasedAllocations=len(f.freed))
    except (AssertionError, ValueError) as error:
        report.update(status='failed', error=str(error), stoppedIp=f'{p.reg("EIP"):08X}', events=f.events.copy())
        raise
    finally:
        report.update(seconds=time.monotonic() - started, arenaReservedBytes=p.allocated)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({key: value for key, value in report.items() if key not in ('events', 'levels')}, sort_keys=True), flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2] == ['--guest'] else run_bounded(Path(__file__), sys.argv[1:]))
