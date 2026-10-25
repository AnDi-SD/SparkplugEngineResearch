"""Qualify shared PC draw-context ABI producers against the live debug EXE."""
import argparse
import hashlib
import json
from pathlib import Path

from qualify_shader_key import ROOT, region

# Existing completed native evidence establishes behavior, calling convention
# and offsets. File identity establishes that the debug build uses those bytes.
REGIONS = [
    ('material_install_CP33', 0x4BE180, 0x2C),
    ('material_state_batch_CP33', 0x4BB890, 0xB0),
    ('material_lighting_CP32', 0x4BDB10, 0x2A0),
    ('color_sources_CP32', 0x4BDDB0, 0xA0),
    ('light_submit_CP43', 0x4BDE50, 0x330),
    ('geometry_submit_CP50', 0x4BC4A0, 0x1D0),
    ('skin_render_region_CP64_CP87', 0x46A240, 0x180),
    ('renderer_vtable_prefix', 0x6F2918, 0x40),
    ('material_vtables', 0x6EF238, 0x40),
    ('light_vtable_prefix', 0x6F0C88, 0x20),
    ('vertex_buffer_vtable', 0x6F05C4, 0x20),
    ('pass_vtable', 0x6E7388, 0x1C),
    ('texture_layer_vtable', 0x6DC91C, 0x20),
    ('standard_layer_vtable', 0x6E7700, 0x20),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(ROOT / 'local-data'):
        raise ValueError('Local evidence output required')
    paths = [ROOT / 'local-data/pc-pristine/WinxClub.exe',
             ROOT / 'local-data/Winx Club/WinxClubDebug.exe']
    data = [path.read_bytes() for path in paths]
    hashes = [hashlib.sha256(raw).hexdigest() for raw in data]
    if hashes != ['3f022480bf55045da4bf692e4bc8862ed38fc024e8a964a558fbdfdf646dfc4f',
                  'c27ea9db4228781a12a90ae808807d4af1397a7e40dd8f5fff28f3c87cc62cdb']:
        raise ValueError('Unqualified EXE')
    rows = []
    for name, address, size in REGIONS:
        parts = [region(raw, address, size) for raw in data]
        rows.append(dict(name=name, address=address, bytes=size, equal=parts[0] == parts[1],
                         sha256=hashlib.sha256(parts[1]).hexdigest()))
    result = dict(schema=1, images=[dict(path=str(p), sha256=h) for p, h in zip(paths, hashes)],
                  regions=rows, scope='Byte identity of specified regions; no native calls, patches or new behavior proof')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as file:
        json.dump(result, file, indent=2)
    print(json.dumps(dict(equal=sum(row['equal'] for row in rows), regions=len(rows))))
    return int(not all(row['equal'] for row in rows))


if __name__ == '__main__':
    raise SystemExit(main())
