"""Qualify native mesh source boundaries against pristine and debug PC EXEs."""
import argparse
import hashlib
import json
from pathlib import Path
from qualify_shader_key import ROOT, region

REGIONS = [
    ('mesh_submit_CP50', 0x4BC670, 0x68),
    ('shared_mesh_initialize', 0x4C29C0, 0x231),
    ('mesh_cpu_initialize', 0x4AA000, 0x34A),
    ('renderer_mesh_slot', 0x6F28C4, 4),
    ('mesh_vtables', 0x6EF32C, 0x30),
    ('index_type_table', 0x6F1A54, 20),
    ('draw_wrapper_CP50', 0x4BC290, 0x178),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(ROOT / 'local-data'):
        raise ValueError('Local evidence output required')
    paths = [ROOT / 'local-data/pc-pristine/WinxClub.exe', ROOT / 'local-data/Winx Club/WinxClubDebug.exe']
    raw = [path.read_bytes() for path in paths]
    hashes = [hashlib.sha256(value).hexdigest() for value in raw]
    if hashes != ['3f022480bf55045da4bf692e4bc8862ed38fc024e8a964a558fbdfdf646dfc4f',
                  'c27ea9db4228781a12a90ae808807d4af1397a7e40dd8f5fff28f3c87cc62cdb']:
        raise ValueError('Unqualified EXE')
    rows = []
    for name, address, size in REGIONS:
        parts = [region(value, address, size) for value in raw]
        rows.append(dict(name=name, address=address, size=size, equal=parts[0] == parts[1],
                         sha256=hashlib.sha256(parts[0]).hexdigest()))
    result = dict(schema=1, images=[dict(path=str(p), sha256=h) for p, h in zip(paths, hashes)], regions=rows,
                  scope='Byte identity. Behavior from existing complete PC mesh payload and renderer-submit evidence; no new emulated game algorithm.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    print(json.dumps(dict(equal=sum(row['equal'] for row in rows), regions=len(rows))))
    return int(not all(row['equal'] for row in rows))


if __name__ == '__main__':
    raise SystemExit(main())
