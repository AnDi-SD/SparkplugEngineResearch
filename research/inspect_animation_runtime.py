#!/usr/bin/env python3
"""Read-only PC evidence checks: actor -> evaluator -> node local PRS.

Addresses and descriptions are analytical labels, not recovered source names.
No PS2 executable, database scan, game process or IDA installation is needed.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
from pathlib import Path
from inspect_serializer_manager import PC_SHA256, image_slice, read_pe, sha256

BODIES = {
    "world-update matrix3 product": (0x00420C00, 0x111, "31C64D057E93F573853E3E85AF6544F74434E977E05BB20A9D05F004BED9C028"),
    "world-update vector-matrix product": (0x00420350, 0x53, "616335B528363D0CDEF8606B518E06A3AD252BC08FC1968F958747A866989F02"),
    "world-update matrix3 copy": (0x0040F1E0, 0x3D, "6BAD60A1F252572E6E21E81991ED0EAD4AB6720ED2310F9EB12063732D9CD25A"),
    "world-update visible tail (bridge resolved by separate guest probe)": (0x0042142E, 0x205, "63E1297450C9BD56E7133267954EA8BB3152692C8FF238738AFFF65B28487B16"),
    "cached world point transform": (0x00420660, 0xA4, "5AFC6A2A38E06D3A0E68FED16A9348A7FE5B5D4F3B261844602DF1A7492956EC"),
    "cached inverse world point transform": (0x00420710, 0xC8, "5031BB42D7710D73FFD15A24F1F0DA054EDE8976A6F9E9E9EF97CF06BC7EE83D"),
    "linear vector key interpolation": (0x00479060, 0x69, "EF1921A642CCB6CF9C3CFF82028CB450062898C6987AF704B89E0D03728F0F03"),
    "node direct application": (0x005FF1B0, 0x9B, "A359184B0E6429A54FDB7690ECA43CC21EAA337BE53CFB0161C3F75CD746E032"),
    "node transition blend": (0x005FF250, 0x185, "5A2E7550A90982159A8524B7F017A0670F2706681583C1233936378CAEDE3F7B"),
    "node controller constructor": (0x005FF400, 0xA1, "51871A47F00162CE9282B2D2CDC59B9A7497DD3D8FF2A4F0A11CB5867B110769"),
    "node controller destructor": (0x005FF4D0, 0x7B, "99A69331A1FEE73056C429057C51F80F7B4C76BC735EDD3021C10A5E5EBF269E"),
    "node controller factory": (0x005FF550, 0x97, "D475128F4E141410B9B2F8BC501A199EDDADC9B9E705CED4096D698876765BCC"),
    "actor tick": (0x005A2380, 0xA71, "BA070E5BDDCED2D826E60BB9C3422D797C9838D04EE1024C555713BF59654911"),
    "evaluator input insertion": (0x005FE9C0, 0x1AA, "BDD430868162F1F655A4A0AB455EBCB1E73D7FB4D3ADD96E1532EEFC700BB388"),
    "evaluator input clear": (0x005FEB70, 0x17, "B94739DBC73A5DEE88E6342EBBA95A99484A9FA1A41C4142ED63A07BD2AAEB02"),
    "track key interval": (0x00478F90, 0xCD, "2D7F926FE8C5000CAECCCD193A975D4DA6301A8FDEE202886F466A1022E41436"),
    "track PRS sampler": (0x00479290, 0x4CF, "69C59A56FF94267A94B3D2B238905F8AE4642EEEB7B5F4E30C434A4FB0015BD1"),
    "quaternion to matrix": (0x004647F0, 0xC4, "3C8BD668E6A7BADD6DD7083F5ACF9D016B96F3052870EF0509A1A0EA782A8DF5"),
    "quaternion interpolation": (0x004648C0, 0x1D2, "FC315E1C29DA2D8380CD8E387598918B616DA6F8D4B49A9A8CD1353394C9F701"),
    "matrix to quaternion": (0x00464CB0, 0x130, "BF2EB27D71B8FBFF86D866E5D539B7D1A0A080EA699ABABA48660CD825B03301"),
    "palette matrix product": (0x00426B00, 0x239, "A02F5C988646F222FDED059766EC50FD7F76E98CF58D0956F98925B727AF4FD8"),
    "matrix copy": (0x0041D330, 0x67, "4CD94EBE73E86781EE221F982F21BBBE6604A1A162DD10BCA3B1689DD5B65561"),
    "skin render and cleanup branches": (0x0046A240, 0x163, "9376D6A7F6E4A5AD2D2AE3E823E61E4B705E8C59D40CC21FC65D5A6403D98F72"),
    "world-matrix builder tail (bridge resolved by separate guest probe)": (0x00461D76, 0xE9, "56C8E50BE9916C74305909EF9F0CBB63A2CA58A52C86A0E0B06E572EF0D7482E"),
}
NODE_VTABLE = (
    0x005FF640, 0x005B7A00, 0x005FF5F0, 0x0040ECE0,
    0x005FF4B0, 0x00408350, 0x00408370, 0x005FF1B0,
    0x005FF250, 0x005FF4C0, 0x005FF4C0, 0x005FF3E0,
)
ACTOR_VTABLE = (
    0x005A35A0, 0x005B7A00, 0x005A3680, 0x00423100,
    0x005A33E0, 0x00408350, 0x00408370, 0x005A2380, 0x005A35C0,
)

def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pc', type=Path, default=root/'local-data/pc-pristine/WinxClub.exe')
    args = parser.parse_args()
    file = args.pc.read_bytes()
    base, sections = read_pe(file)
    failures, checks = [], 0
    def read(va: int, size: int) -> bytes:
        return image_slice(file, sections, va-base, size)
    def check(label: str, actual: object, expected: object) -> None:
        nonlocal checks
        checks += 1
        passed = actual == expected
        print(f"{'OK' if passed else 'FAIL'} {label}")
        if not passed:
            failures.append(f'{label}: {actual!r} != {expected!r}')
    check('pristine PC SHA', sha256(file), PC_SHA256)
    for label, (va, size, digest) in BODIES.items():
        check(label, hashlib.sha256(read(va,size)).hexdigest().upper(), digest)
    check('complete node controller vtable', struct.unpack('<12I',read(0x00711444,48)), NODE_VTABLE)
    check('node controller vtable boundary', read(0x00711474,17), b'spNodeController\x00')
    check('complete actor vtable', struct.unpack('<9I',read(0x00703F80,36)), ACTOR_VTABLE)
    check('actor vtable boundary', read(0x00703FA4,8), b'spActor\x00')
    check('node evaluator getter', read(0x005FF4C0,4), bytes.fromhex('8B 41 14 C3'))
    check('actor registration getter', read(0x005A33E0,6), bytes.fromhex('B8 80 64 76 00 C3'))
    check('node controller registration getter', read(0x005FF4B0,6), bytes.fromhex('B8 F0 8E 76 00 C3'))
    for label, at, target in (
        ('evaluator calls PRS sampler',0x005FEC21,0x00479290),
        ('actor joins state and track into evaluator',0x005A1D26,0x005FE9C0),
        ('direct application calls quaternion setter',0x005FF218,0x00420640),
        ('blend calls quaternion setter',0x005FF3A2,0x00420640),
        ('skin calls world-matrix builder',0x0046A29A,0x00461D70),
        ('skin multiplies inverse-bind with world',0x0046A2AA,0x00426B00),
    ):
        call = read(at,5)
        check(label, (call[0], at+5+struct.unpack_from('<i',call,1)[0]), (0xE8,target))
    check('direct evaluator virtual +0x20',read(0x005FF1DE,3),bytes.fromhex('FF 50 20'))
    check('blended evaluator virtual +0x20',read(0x005FF27E,3),bytes.fromhex('FF 50 20'))
    check('actor controller direct +0x1C',read(0x005A2D5F,3),bytes.fromhex('FF 56 1C'))
    check('actor controller blend +0x20',read(0x005A2D4D,3),bytes.fromhex('FF 56 20'))
    check('quaternion largest-axis cycle',struct.unpack('<3I',read(0x00740350,12)),(1,2,0))
    check('node world-update virtual +0x30',struct.unpack('<I',read(0x006DC524,4))[0],0x00421420)
    check('node world-update protected bridge',read(0x00421428,6),bytes.fromhex('FF 25 10 15 3B 01'))
    check('node world-update child recursion',read(0x00421611,3),bytes.fromhex('FF 52 30'))
    print(f"RESULT {'FAIL' if failures else 'PASS'} checks={checks-len(failures)}/{checks}")
    for failure in failures:
        print(failure)
    return int(bool(failures))

if __name__ == '__main__':
    raise SystemExit(main())
