#!/usr/bin/env python3
"""PC SAN reader identity and bounded code-window hash anchors."""
from pathlib import Path
import struct
from inspect_serializer_manager import PC_SHA256, sha256, read_pe, image_slice

WINDOWS = {
    'complete unprotected reader': (0x43ECC0, 0x6DC, 'E6288678FDDB2996FE94A6B903F542B2A2137BE8057302334538FE84E9DD61C6'),
    'tag-name protected entry and neighboring identity window': (0x43D9D0, 0x87, '71E4174C2CFC2BFE991BAA1B6462B3B6A23EE29E56F743B26DA989D7C1260D09'),
    'reserve plus-one path': (0x43EFEE, 0x1F, 'E0B0A6D25C8D71AF0F56806DCCADE2A04A35147EBE4F989F1C81973C198074A2'),
    'null-header success exit': (0x43ED4A, 0x68, '79397E473026B27DB102AEDEA42A01A93CC2DBD29BD319714845F5E0C292C857'),
    'data-block header tail window': (0x472909, 0x179, 'BDA781242EC8F04CDC315EAB85D142B512ED82B88D545934ABFCBAD74DD716C3'),
}
TABLES = {
    0x6E0B0C: (0x43DB70, 0x5B7A00, 0x43DB20, 0x40ECE0, 0x43DA20, 0x408350, 0x408370),
    0x6E0B00: (0x43DFE0, 0x5A7DB0, 0x43ECC0),
}


def main():
    raw = (Path(__file__).resolve().parents[1] / 'local-data/pc-pristine/WinxClub.exe').read_bytes()
    if sha256(raw) != PC_SHA256:
        raise ValueError('non-pristine image')
    base, sections = read_pe(raw)
    for label, (address, size, digest) in WINDOWS.items():
        if sha256(image_slice(raw, sections, address - base, size)) != digest:
            raise AssertionError(label)
    for address, expected in TABLES.items():
        data = image_slice(raw, sections, address - base, 4 * len(expected))
        if struct.unpack('<' + 'I' * len(expected), data) != expected:
            raise AssertionError(hex(address))
    print('PASS 8/8: full SAN reader hash/identity anchors')


if __name__ == '__main__':
    main()
