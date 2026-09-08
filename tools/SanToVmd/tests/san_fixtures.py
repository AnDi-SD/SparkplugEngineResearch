"""Test-only FFPS construction. Evaluation always goes through the C++ core."""
import struct
import san_to_vmd as converter


def field(kind, payload):
    return bytes([0xE0 | kind]) + struct.pack('<I', len(payload)) + payload


def container(objects):
    table, body = bytearray(), bytearray()
    for number, name, kind, fields in objects:
        name = name.encode('ascii') + b'\0'
        obj = struct.pack('<I4s', kind, b'SBOO') + fields + b'\0'
        table += struct.pack('<IH', number, len(name)) + name
        table += struct.pack('<III', kind, len(body), len(obj))
        body += obj
    table += bytes(4)
    start = 32 + len(table)
    return struct.pack('<4s7I', b'FFPS', 0x26, 0, start+len(body), 2, start, len(body), len(objects)) + table + body


def wire(representation, times, rows):
    return (struct.pack('<II', representation, len(times)) +
            struct.pack('<' + 'f'*len(times), *times) +
            b''.join(struct.pack('<' + 'f'*len(row), *row) for row in rows))


def animation(tracks, duration=12):
    body = field(0, struct.pack('<f', duration))
    for name, roles in tracks:
        body += b''.join(field(role, payload) for role, payload in roles.items())
        name = name.encode('ascii') + b'\0'
        body += field(1, struct.pack('<H', len(name)) + name)
    return container([(1, 'synthetic', 0x56EE563A, body)])


def channel(payload, role):
    owner = converter.native.Animation(animation([('Root', {role: payload})]))
    return owner.tracks[0].channels[role-2]


def clip(tracks, duration=12):
    owner = converter.native.Animation(animation(tracks, duration))
    selected = {}
    for track in owner.tracks:
        roles = selected.setdefault(track.name, {})
        for role, value in track.channels.items():
            if value.source_keys:
                roles.setdefault(role+2, value)
    return converter.Clip(owner.duration, selected, owner.tags, [], owner)
