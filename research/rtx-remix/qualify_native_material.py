"""Read-only qualification of the ordinary native material texture identity."""
import argparse
import hashlib
import json
import struct
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
    for name, address, size in [('ordinaryVtable', 0x6E8440, 40), ('destructorVtableWrite', 0x467D0D, 6),
                                ('textureGetter', 0x467BE0, 4)]:
        parts = [region(b, address, size) for b in raw]
        rows.append(dict(name=name, address=address, size=size, equal=parts[0] == parts[1],
                         sha256=hashlib.sha256(parts[0]).hexdigest(), hex=parts[0].hex()))
    vtable = struct.unpack('<10I', region(raw[0], 0x6E8440, 40))
    if (vtable[0] != 0x467FE0 or vtable[7] != 0x467BE0 or
            region(raw[0], 0x467D0D, 6).hex() != 'c70640846e00' or
            region(raw[0], 0x467BE0, 4).hex() != '8b4134c3'):
        raise ValueError('Ordinary texture identity/getter differs')
    result = dict(schema=1, images=[dict(path=str(p), sha256=h) for p, h in zip(paths, hashes)],
                  regions=rows, vtable=list(vtable), getter='mov eax,[ecx+34]; ret',
                  scope='Static byte identity of ordinary MaterialTexture. No protected factory execution or new emulation; source behavior uses existing CP31/32/33/36 and material texture evidence.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    print(json.dumps(dict(equal=sum(r['equal'] for r in rows), regions=len(rows))))
    return int(not all(r['equal'] for r in rows))


if __name__ == '__main__':
    raise SystemExit(main())
