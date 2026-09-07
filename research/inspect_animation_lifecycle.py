#!/usr/bin/env python3
"""Hash/identity anchors for PC animation object/track lifetime evidence."""
from pathlib import Path
import struct
from inspect_serializer_manager import PC_SHA256,sha256,read_pe,image_slice

BODIES={
    'animation factory resolved':(0x13D1E00,0x7E,'404D033E07536276A3B50D2F95572B61E239B37D57B1585D4727E025C6C75DF1'),
    'animation constructor resolved':(0x13C64B0,0x13B,'B43DDE87E05A1B53BDF388A98DE647932B2FA68BE16B3ADD8006AC9F6D9B34DE'),
    'append successful path':(0x13C91A0,0x138,'1FF5554A91A981CD630E8C2E693A1A46413CB0F9AE6A971C298D517B41B41727'),
    'key release resolved':(0x13D2850,0x12E,'0AA2D82FAC82427BE1F8F62532DBC6C60BDD0DC37B58E82302ECDE38AF82B368'),
    'track maximum time':(0x478E30,0x154,'7211B06FA17C6EDBE715BAF0767D2B98233CB1B9DAC51E547FE5C26F9D7C9820'),
    'base track time':(0x493060,7,'9BF8EA0BA148D143E3370BBC5228494C3C89E46904C2C29722BEBBEA50693CDF'),
    'base track factory resolved':(0x49EE20,0x87,'F81753EECD09A52130416129301A7E17D6179F1550ACA62F574DA44BBE02DC29'),
    'anim track registration':(0x6D3EB0,0x26,'019BD5025E7EF8622B00F166F35A01A25ACACB1E8C9A5BEA88FD44F2AF372E6E'),
    'track registration':(0x6D4980,0x26,'ED6EBD52BE9AA161BC30F2887947E91B2704EE5572A02010C8B6CEE2FED419B7'),
    'stable tag insertion':(0x4305F0,0x53,'1B13EBDAA9DA3400C72366A986FD493DD786AFC8B8F3C7A8074BC8DCBC2D2B93'),
    'descriptor pool init':(0x417760,0x8B,'AC60819E87D929919EC229A4CFE577A40DBECA2038B8AF4ECBC170B49304EE0E'),
    'descriptor pool teardown':(0x417690,0x85,'C0C1328BE948FBE40E234E31FE72A29FFC3A4BFD70EB533F4797EE59842D76B7'),
    'named destructor':(0x413090,0x52,'9CBC3901A9FB4E1E9188AB10183E1E17DF3B6AB42F92840257B244F687E8876A'),
    'debug table consumer':(0x41D4E0,0x3D,'C408F7388FBE5D1848EC2854129D792374A9B42B5800FF89B951618DBD5873A4'),
}
TABLES={
    0x6EAA24:(0x479A40,0x5B7A00,0x479240,0x413120,0x478E20,0x408350,0x408370,0x478E30,0x479760),
    0x6ECBA4:(0x493140,0x5B7A00,0x4930E0,0x413120,0x493050,0x408350,0x408370,0x493060,0x48EAA0),
    0x6E0AE4:(0x43DA60,0x5B7A00,0x4A1BF0,0x40ECE0,0x40E930,0x408350,0x408370),
}

def main():
    raw=(Path(__file__).resolve().parents[1]/'local-data/pc-pristine/WinxClub.exe').read_bytes()
    if sha256(raw)!=PC_SHA256:raise ValueError('non-pristine image')
    base,sections=read_pe(raw)
    def data(a,n):return image_slice(raw,sections,a-base,n)
    for label,(a,n,digest) in BODIES.items():
        if sha256(data(a,n))!=digest:raise AssertionError(label)
    for a,expected in TABLES.items():
        if struct.unpack('<'+'I'*len(expected),data(a,len(expected)*4))!=expected:raise AssertionError(hex(a))
    assert data(0x6eaa48,12)==b'spAnimTrack\0'
    assert data(0x6ecbc8,8)==b'spTrack\0'
    print(f'PASS {len(BODIES)+len(TABLES)+3}/{len(BODIES)+len(TABLES)+3}: lifecycle hash/identity anchors')
    return 0

if __name__=='__main__':raise SystemExit(main())
