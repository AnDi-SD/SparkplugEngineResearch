"""Qualify the camera source hook against pristine and debug PC binaries."""
import argparse
import hashlib
import json
from pathlib import Path
from qualify_shader_key import ROOT, region


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(ROOT / 'local-data'):
        raise ValueError('Local evidence output required')
    paths = [ROOT / 'local-data/pc-pristine/WinxClub.exe', ROOT / 'local-data/Winx Club/WinxClubDebug.exe']
    raw = [p.read_bytes() for p in paths]
    hashes = [hashlib.sha256(b).hexdigest() for b in raw]
    if hashes != ['3f022480bf55045da4bf692e4bc8862ed38fc024e8a964a558fbdfdf646dfc4f',
                  'c27ea9db4228781a12a90ae808807d4af1397a7e40dd8f5fff28f3c87cc62cdb']:
        raise ValueError('Unqualified EXE')
    rows = []
    for name, address, size in [('apply', 0x427D40, 0x5A), ('viewSetter', 0x4BBB20, 0x36),
                                ('projectionSetter', 0x4BBAE0, 0x36)]:
        parts = [region(b, address, size) for b in raw]
        rows.append(dict(name=name, address=address, size=size, equal=parts[0] == parts[1],
                         sha256=hashlib.sha256(parts[0]).hexdigest(), head=parts[0][:10].hex()))
    if rows[0]['head'] != '568bf1f6862402000001':
        raise ValueError('Camera hook prefix differs')
    result = dict(schema=1, images=[dict(path=str(p), sha256=h) for p, h in zip(paths, hashes)],
                  regions=rows, hookBytes=10,
                  displacedInstructions=['push esi', 'mov esi,ecx', 'test byte ptr [esi+0x224],1'],
                  scope='Static byte identity and hook boundary. Original behavior from existing complete CP6/CP47 evidence; no new emulation.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    print(json.dumps(dict(equal=sum(r['equal'] for r in rows), regions=len(rows))))
    return int(not all(r['equal'] for r in rows))


if __name__ == '__main__':
    raise SystemExit(main())
