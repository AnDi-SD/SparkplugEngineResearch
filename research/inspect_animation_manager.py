#!/usr/bin/env python3
"""Read-only pristine PC anchors for original animation-manager evidence."""
import argparse
import hashlib
from pathlib import Path
import struct
from inspect_serializer_manager import PC_SHA256, read_pe, image_slice, sha256

ANCHORS = {
    'controller constructor': (0x4c4210, 0x69, '1FCF41E586889F54052274E40E240D9A51AF5BE0E8D2ADB99601ED5BAFFF2898'),
    'controller copy': (0x419aa0, 0x82, '753204307AE53FF9F65BDB0C9F8B71C608E2B9F46BFA5708991775DFB5CC60CD'),
    'actor clone': (0x5a3680, 0x49, '843E02524C1A76C3E61CAF1C1E585ED208D8CE3F4FE406303C42956F23AE4831'),
    'registration': (0x6d35b0, 0x26, '288A810B1B1C09935655F932705AB5FDAD23D3482B8EA2763C88A0F2FE7324EE'),
    'factory': (0x13dd4e0, 0x8c, '14287C8A3D27873568DC28DDFE20F1B59E48F1A1D2347B9FA3BF96EE5B08BC47'),
    'constructor': (0x13ca630, 0xa2, '36129D7A3E49FEA5CDFB8846A2A8EB993D0BB892E6BA49BC14513A92AE034959'),
    'destructor': (0x13d8b50, 0xa4, 'E8E0168C2ED168AB4EC30BBBD3B646E95C2D02AB9255AD277724CC2177F69B91'),
    'name bind': (0x13b8300, 0x2af, '89175725976E85E55F9891FAA3B544A8D08B9324FA0432F7DF23C49750CC086D'),
    'name unbind': (0x13d8400, 0x198, '8F6B9099BF27C3DA2B4B6BB70ACB1ED32ED3999E70E785372BA6A1C8E08B71B2'),
    'register controller': (0x405c10, 0x35, '9751B26178798133CF948D93CE27FD9B30B6101E73D5BDB1B3A8DC57F39A1D5A'),
    'unregister controller': (0x44ae20, 0x4f, 'DE4B9A80186D907C4A7D4396760E5E4FFB1D8139249BC9DE6C7EEA59349D3F59'),
    'frame': (0x4535a0, 0x4c, '73845044E91D0D89A6C3456097361186B756104AB48706795CB37DA22C0E91F9'),
    'attach evaluator': (0x4545f0, 0x48, '3CCCFC420C76B1B01CC2032805CC320165CB608D9E9DE9D1B39DBC0CFE5DBCD9'),
    'detach evaluator': (0x453e90, 0x18, 'F9D36898571E9C05B2E5B459C9AEFA8DD81F9B33D754DCF881E90B04BADB297C'),
    'clone': (0x4546a0, 0x49, '7EDB83FCD0C8C261A2B522A47A1F3E21B9942B9BBCE9AFC28DA9D6257E3A088F'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pc', type=Path, default=Path(__file__).resolve().parents[1] /
                        'local-data/pc-pristine/WinxClub.exe')
    args = parser.parse_args()
    raw = args.pc.read_bytes()
    base, sections = read_pe(raw)
    checks, failures = 0, 0

    def check(value, label):
        nonlocal checks, failures
        checks += 1
        failures += not value
        print(('OK ' if value else 'FAIL ') + label)

    def data(address, size):
        return image_slice(raw, sections, address - base, size)

    check(sha256(raw) == PC_SHA256, 'whole pristine PE SHA-256')
    for label, (address, size, digest) in ANCHORS.items():
        check(hashlib.sha256(data(address, size)).hexdigest().upper() == digest, label + ' original span')
    check(struct.unpack('<7I', data(0x6e6e30, 28)) ==
          (0x4545d0, 0x5b7a00, 0x4546a0, 0x40ece0, 0x454360, 0x408350, 0x408370),
          'complete seven-slot manager vtable')
    check(data(0x6e6e4c, 19) == b'spAnimationManager\0', 'vtable ends at original class string')
    check(data(0x454360, 6) == bytes.fromhex('B8 88 F8 75 00 C3'), 'registration getter')
    registration = data(0x6d35b0, 0x26)
    check(bytes.fromhex('68 A1 52 53 41 68 C1 4C 21 5D B9 88 F8 75 00') in registration,
          'original class and direct root base IDs')
    print(f'RESULT {"FAIL" if failures else "PASS"} {checks-failures}/{checks}')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
