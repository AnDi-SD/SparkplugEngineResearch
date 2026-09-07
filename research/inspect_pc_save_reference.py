#!/usr/bin/env python3
"""PC-only save-side index/reference function fingerprints."""
from pathlib import Path
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256

BODIES=(
    (0x466fa0,0xe4,'CD454FF36DAF3C7419F7C068B47CA102341557CDD580442B5F7E7B7D038D20CA'),
    (0x47dd80,0xb5,'BADCD11B5714FD264993653C619BFCB6FF3CCC87EE284BA946C6219C94BEE495'),
    (0x467260,0x5f,'304A8080E63E2B6B225735C2070B1B2C4A1999215247D016319050D43E0A5F5C'),
    (0x4672c0,0x3f,'09BE94CD57175B9ED944EE9BBB8C88310128BA5969CD2E954A4E5324CE058509'),
    (0x467350,0x1ff,'DB285B0D68070C77813BB8B3858E62004676BB44CD070C51096F5479D2EB8B4F'),
    (0x422530,0x1a,'80A89475D220AE09CE87C5FC905C43CA178B4C2D005F0006835A4F17B4D6414D'),
    (0x4664f0,0x2f,'EC316C1B479996FAFEC407483733CABC767C3EF7605C0EBADAEC126543406A81'),
    (0x5a7db0,5,'F4C6D7AE520F88AECB3EA65952E885437FA4A6CE4B5C3439A161D1C5D8E42863'),
)


def main():
    raw=(Path(__file__).resolve().parents[1]/'local-data/pc-pristine/WinxClub.exe').read_bytes()
    if sha256(raw)!=PC_SHA256:raise AssertionError('changed PC fingerprint')
    base,sections=read_pe(raw)
    for address,size,digest in BODIES:
        if sha256(image_slice(raw,sections,address-base,size))!=digest:raise AssertionError(hex(address))
    print('PASS 9/9: original PC save-reference/index byte fingerprints')
    return 0


if __name__=='__main__':raise SystemExit(main())
