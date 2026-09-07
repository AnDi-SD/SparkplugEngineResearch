#!/usr/bin/env python3
"""Read-only hashes for the PC SAN payload/preparation/sampling boundary."""
from pathlib import Path
from inspect_serializer_manager import PC_SHA256,sha256,read_pe,image_slice

BODIES={
    'interval':(0x478F90,0xCD,'2D7F926FE8C5000CAECCCD193A975D4DA6301A8FDEE202886F466A1022E41436'),
    'PRS sampler':(0x479290,0x4CF,'69C59A56FF94267A94B3D2B238905F8AE4642EEEB7B5F4E30C434A4FB0015BD1'),
    'payload reader tail':(0x43DBA3,0x209,'E7FA840EA38790EB60231CA6059D5FCF996EF5FD7FB70568C49B122DE8261C9A'),
    'key attach':(0x479830,0x1B1,'93DCE6F55A3C4D73ADBF905A8149AC48EA330D7A788D4C9ABDCA41A1E552D236'),
    'scalar coefficients':(0x493160,0x124,'DC009F987090FCFC92D25D1CB51E794C3D3EF552FF3F1393A829529FB99BB948'),
    'vector coefficients':(0x493290,0x12D,'8419BD58855FB5D3670E775698D08E7CED74BB0CA26819C3F989E1D01832F133'),
    'quaternion prepare relocated body':(0x13C1220,0xDC,'9B63E5677B90FAEAE912C4787BEC21012468E46DAB07EADBF4F7F7C49715D303'),
    'squad':(0x464B20,0x70,'CF16FFD5792C7790C36D838116005CE1A3396669BF685BC61948EBED504E78E4'),
    'quaternion log':(0x464C10,0x69,'452061886080379A33D1088DC95954A59A25B9A42409933153B4F617D36DF97D'),
    'quaternion exp':(0x464B90,0x7E,'F873DEECBA56D0773927B23EA5F14A5D64EC3325A122B7E4793BA2B46BA031D8'),
    'control helper':(0x464DE0,0xF9,'24E74E32CC13E0CEDB57CF20B8BC3CD495C3CE14521F2D3E13BB9302E38C4E25'),
}

def main():
    root=Path(__file__).resolve().parents[1]
    raw=(root/'local-data/pc-pristine/WinxClub.exe').read_bytes()
    if sha256(raw)!=PC_SHA256:raise ValueError('non-pristine image')
    base,sections=read_pe(raw)
    for label,(address,size,digest) in BODIES.items():
        if sha256(image_slice(raw,sections,address-base,size))!=digest:raise AssertionError(label)
    print(f'PASS {len(BODIES)+1}/{len(BODIES)+1}: PC SHA and SAN key bodies')
    return 0

if __name__=='__main__':raise SystemExit(main())
