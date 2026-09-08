#!/usr/bin/env python3
"""Bounded original-PC reader/sampler comparisons for the actual VMD converter.

Only stream I/O, descriptor allocation and CRT acos are fixtures. No original
engine method is replaced by the Python decoder being tested. Twelve synthetic
tracks and selected real tracks cover encodings without rescanning the corpus.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import time

from pc_instruction_emulator import ROOT, PcInstructions, run_bounded
from probe_pc_animation_keys import bits, install_acos_seams, euler
from inspect_pc_san_keys import inspect, key_payload

sys.path.insert(0, str(ROOT/'tools/SanToVmd'))
import san_to_vmd as converter


def wire(rep, key_times, rows):
    return (struct.pack('<II', rep, len(key_times)) +
            struct.pack('<'+'f'*len(key_times), *key_times) +
            b''.join(struct.pack('<'+'f'*len(row), *row) for row in rows))


def fixtures():
    """Plain wire records; expected PRS comes exclusively from the PC image."""
    for rep in (1, 2, 3, 4):
        for count in (2, 3):
            channels = {}
            for role in range(3):
                axes = []
                for axis in range(3 if rep >= 3 else 1):
                    key_times = tuple((i+1)*(.375 + axis*.125) for i in range(count))
                    rows = []
                    for i in range(count):
                        if rep >= 3:
                            value = 1.0 if role == 2 else ((i+1)*(axis+1)*.15 if role == 1 else (i-1)*(axis+2)*.75)
                            rows.append((value,) if rep == 3 else
                                        (value, 0 if role == 2 else .125*(axis+1),
                                         0 if role == 2 else .375*(i+1), 91, 92))
                        else:
                            value = ((1, 1, 1) if role == 2 else euler((.2*i, -.3*i, .4*i))
                                     if role == 1 else (i*2-1, i*3+2, 1-i*.5))
                            rows.append(value if rep == 1 else (*value, 91, 92, 93, 94)
                                        if role == 1 else
                                        (*value, *((0,)*6 if role == 2 else (.5, -.25, .75, 1.5, .5, -.5)),
                                         91, 92, 93, 94, 95, 96))
                    axes.append(wire(rep, key_times, rows))
                channels[role] = b''.join(axes)
            yield f'representation-{rep}-count-{count}', channels, (-.5, 0, .375, .5, .625, .75, 1, 1.125, 1.5, 2, .5)

    # Mixed scalar descriptors, unequal counts and independent axis times.
    mixed = (wire(3, (0, .5, 1), ((0,), (.2,), (.4,))) +
             wire(4, (.125, .75), ((.1, 0, .3, 91, 92), (.6, .2, 0, 91, 92))) +
             wire(3, (0,), ((.2,),)))
    yield 'mixed-scalar-rotation', {1: mixed}, (0, .125, .25, .5, .75, 1, 2)
    for angle in (.0005, .003):
        value = (0, 0, math.sin(angle*.5), math.cos(angle*.5))
        yield f'quaternion-small-angle-{angle}', {1: wire(1, (0, 1, 2), ((0, 0, 0, 1), value, value))}, (0, .25, .75, 1, 1.5, 2)
    yield 'single-cubic-quaternion', {1: wire(2, (.25,), ((0, 0, .6, .8, 91, 92, 93, 94),))}, (0, .25, 1)


def vmd_fixture_bytes(name, channels):
    """Explicit synthetic clip, with pool counts and two required Icy bones."""
    def field(kind, payload):
        return (bytes([0xe0 | kind]) if kind < 31 else bytes([0xff, kind])) + struct.pack('<I', len(payload)) + payload

    pools = [0]*7
    track_fields = bytearray()
    for bone, roles in (('Pelvis', channels), ('L_Bicep', {1: channels[1]})):
        for role, payload in roles.items():
            track_fields += field(role+2, payload)
            for axis in key_payload(payload, role):
                rep, count = axis['representation'], axis['count']
                pool = 0 if rep == 3 else 1 if rep == 4 else (4 if rep == 1 else 5) if role == 1 else 2 if rep == 1 else 3
                pools[pool] += count
                pools[6] += count
        encoded = bone.encode('ascii') + b'\0'
        track_fields += field(1, struct.pack('<H', len(encoded)) + encoded)
    body = struct.pack('<I4s', 0x56ee563a, b'SBOO') + field(0, struct.pack('<f', 2))
    body += b''.join(field(i+6, struct.pack('<I', value)) for i, value in enumerate(pools))
    body += field(64, struct.pack('<I', 2)) + track_fields + b'\0'
    encoded = name.encode('ascii') + b'\0'
    table = struct.pack('<IH', 1, len(encoded)) + encoded + struct.pack('<III', 0x56ee563a, 0, len(body)) + bytes(4)
    start = 32+len(table)
    return struct.pack('<4s7I', b'FFPS', 0x26, 0, start+len(body), 2, start, len(body), 1) + table + body


def emit_vmd_fixtures(p, directory):
    directory.mkdir(parents=True, exist_ok=True)
    results = []
    selected = {'representation-2-count-3', 'representation-3-count-3',
                'representation-4-count-3', 'mixed-scalar-rotation'}
    for name, channels, _ in fixtures():
        if name not in selected:
            continue
        native = NativeTrack(p, channels)
        path = directory/(name+'.san')
        raw = vmd_fixture_bytes(name, channels)
        path.write_bytes(raw)
        samples = []
        for frame in range(61):
            seconds = struct.unpack('<f', struct.pack('<f', frame/30))[0]
            original, flags = native.sample(seconds)
            tracks = {'Pelvis': {str(role+2): value for role, value in enumerate(original) if flags[role]},
                      'L_Bicep': {'3': original[1]}}
            samples.append({'frame': frame, 'seconds': seconds, 'tracks': tracks})
        results.append({'san': path.name, 'sha256': hashlib.sha256(raw).hexdigest().upper(),
                        'duration': 2, 'source': 'synthetic wire, original reader and sampler outputs', 'samples': samples})
    return results


class NativeTrack:
    def __init__(self, machine, channels):
        p = self.p = machine
        p.reset_arena()
        self.track = p.allocate(0x44)
        serializer, animation = p.allocate(0x4c), p.allocate(0x6c)
        p.put_uint(self.track+0x40, animation)
        # Fixed 4KiB pools cover this probe's <=40 keys/channel. Not a claim
        # about production animation sizes or the native allocator policy.
        for index in range(7):
            p.put_uint(animation+0x38+index*4, p.allocate(4096))
        stream, vtable = p.allocate(4), p.allocate(0x3c)
        p.put_uint(stream, vtable)
        p.put_uint(vtable+0x30, 0x401000)
        payload, cursor = [b''], [0]

        def read(machine):
            stack = machine.reg('ESP')
            destination, amount = machine.uint(stack+4), machine.uint(stack+8)
            assert machine.reg('ECX') == stream
            if cursor[0]+amount > len(payload[0]):
                machine.fixture_return(8, 0)
                return
            machine.mu.mem_write(destination, payload[0][cursor[0]:cursor[0]+amount])
            cursor[0] += amount
            machine.fixture_return(8, 1)

        def allocate(machine):
            assert machine.reg('ECX') == animation+0x58
            machine.fixture_return(0, machine.allocate(16))

        p.seams[0x401000], p.seams[0x417510] = read, allocate
        try:
            for role, data in channels.items():
                assert len(data) <= 4096
                payload[0], cursor[0] = data, 0
                p.run(0x43db90, serializer, [stream, self.track, role, animation])
                assert p.reg('EAX') & 0xff == 1 and cursor[0] == len(data)
        finally:
            p.seams.pop(0x401000)
            p.seams.pop(0x417510)
        self.cache, self.outputs, self.flags = p.allocate(36), p.allocate(40), p.allocate(3)

    def sample(self, seconds):
        p = self.p
        p.put_floats(self.outputs, (*converter.ZERO, *converter.IDENTITY, 1, 1, 1))
        p.run(0x479290, self.track, [bits(seconds), self.cache, self.cache+12, self.cache+24,
              self.outputs, self.outputs+12, self.outputs+28, self.flags, self.flags+1, self.flags+2])
        return (p.floats(self.outputs, 3), p.floats(self.outputs+12, 4), p.floats(self.outputs+28, 3)), bytes(p.mu.mem_read(self.flags, 3))


def guest(output):
    if not output.resolve().is_relative_to(ROOT/'local-data/results'):
        raise ValueError('Report must stay under local-data/results')
    started = time.perf_counter()
    p = PcInstructions()
    install_acos_seams(p)
    results = []
    sources = []
    cases = list(fixtures())
    for name in ('barrel.san', 'bbush.san', 'bflower.san', 'bw.san'):
        summary, tracks = inspect(ROOT/'local-data/pc-pristine/Media/Animations'/name)
        sources.append(summary)
        # Only two informative tracks/file: rare scale, one-key and animated.
        chosen = tracks[:1]
        if name == 'bbush.san':
            chosen = [next(t for t in tracks if any(c['representation'] == 2 for c in t['roles'][2]['channels']))]
        elif name == 'bflower.san':
            chosen.append(next(t for t in tracks if t['roles'][1]['channels'][0]['count'] > 1))
        for track in chosen:
            cases.append((f"{name}:{track['name']}", {role: row['payload'] for role, row in track['roles'].items()},
                          (0, 1/30, summary['duration']*.5, summary['duration'], summary['duration']+1)))
    for name in ('xiwa.san', 'xiru.san', 'xiid.san', 'xibhu.san'):
        summary, tracks = inspect(ROOT/'local-data/pc-pristine/Media/Characters/Icy'/name)
        sources.append(summary)
        for bone in ('Pelvis', 'L_Bicep', 'L_UpperArm'):
            track = next(t for t in tracks if t['name'] == bone)
            cases.append((f'{name}:{bone}', {role: row['payload'] for role, row in track['roles'].items()},
                          (0, 1/30, summary['duration']*.5, summary['duration'], summary['duration']+1)))
    for name, channels, seconds in cases:
        native = NativeTrack(p, channels)
        decoded = {}
        rejected_scale = False
        for role, payload in channels.items():
            try:
                decoded[role] = converter.read_curve(payload, role+2)
            except converter.ConversionError as error:
                if role != 2 or 'масштаба' not in str(error):
                    raise
                rejected_scale = True
                # Actual converter must reject non-VMD scale. The same packed
                # vector math is additionally compared in its position role.
                decoded[role] = converter.read_curve(payload, 2)
        rows, maximum = [], 0.0
        for seconds_value in seconds:
            # Original inputs and converter use the same float32 time.
            t = struct.unpack('<f', struct.pack('<f', seconds_value))[0]
            original, flags = native.sample(t)
            actual = []
            for role, fallback in enumerate((converter.ZERO, converter.IDENTITY, (1, 1, 1))):
                curves = decoded.get(role)
                expected_flag = bool(curves and curves[0].times)
                assert flags[role] == expected_flag, (name, role, 'validity')
                value = converter.sample(curves, t, fallback, role == 1)
                actual.append(value)
                error = max(abs(a-b) for a, b in zip(value, original[role]))
                maximum = max(maximum, error)
                assert all(math.isclose(a, b, rel_tol=4e-5, abs_tol=4e-5) for a, b in zip(value, original[role])), (name, role, t, original[role], value)
            rows.append({'seconds': t, 'original': original, 'converter': actual, 'validity': list(flags)})
        results.append({'case': name, 'channels': {str(role): hashlib.sha256(data).hexdigest().upper() for role, data in channels.items()},
                        'non_vmd_scale_rejected': rejected_scale, 'max_component_error': maximum, 'samples': rows})
    vmd_fixtures = emit_vmd_fixtures(p, output.parent/'vmd-fixtures')
    report = {'status': 'passed', 'pc_sha256': hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
              'converter_sha256': hashlib.sha256(Path(converter.__file__).read_bytes()).hexdigest().upper(),
              'probe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
              'seconds': time.perf_counter()-started, 'case_count': len(results),
              'samples': sum(len(row['samples']) for row in results), 'sources': sources, 'cases': results,
              'vmd_fixtures': vmd_fixtures, 'vmd_fixture_native_samples': sum(len(row['samples']) for row in vmd_fixtures),
              'max_component_error': max(row['max_component_error'] for row in results),
              'seams': ['stream read', '16-byte descriptor pool allocation', 'CRT acos (two observed ABI entries)'],
              'host_guards': ['bounds/finite/strictly increasing times', 'unit quaternion normalization and log roundoff clamp',
                              'bounded one-key neighbor and unused coefficient handling', 'VMD scale rejection'],
              'native_endpoint_policy': 'PC 0x478F90: count 2 at/after last => first key; count >=3 => final interval at u=1'}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('status', 'case_count', 'samples', 'max_component_error', 'seconds')}))
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--guest', action='store_true')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    raise SystemExit(guest(args.output) if args.guest else run_bounded(Path(__file__), ['--output', str(args.output)]))
